#include "actor_runtime/managed_lua_actor.hpp"

#if ESP_IDF
#include <esp_attr.h>
#endif
#include <base64.h>
#include <blake2.h>

#include <array>
#include <cassert>
#include <chrono>
#include <ctime>
#include <string_view>

#include "controllers/telemetry_data.hpp"
#include "support/logger.hpp"

#if CONFIG_BENCHMARK_ENABLED
#include "remote/remote_connection.hpp"
#endif

namespace uActor::ActorRuntime {

ManagedLuaActor::~ManagedLuaActor() {
  uActor::Support::Logger::trace("MANAGED-LUA-ACTOR", "LUA Actor Stopped.");
  if (initialized()) {
    lua_pushnil(state);
    lua_setglobal(state, std::to_string(id()).data());
    lua_pushnil(state);
    lua_setglobal(state,
                  (std::string("state_") + std::to_string(id())).c_str());
    lua_gc(state, LUA_GCCOLLECT, 0);
  }
}

ManagedActor::RuntimeReturnValue ManagedLuaActor::receive(
    PubSub::Publication&& m) {
#if CONFIG_BENCHMARK_BREAKDOWN
  if (m.get_str_attr("type") == "ping") {
    testbed_stop_timekeeping_inner(6, "scheduling");
  }
#endif
  if (!initialized()) {
    Support::Logger::warning(
        "MANAGED-LUA-ACTOR",
        "Receive: Actor not initialized, can't process message");
    return RuntimeReturnValue::NOT_READY;
  }

  lua_getglobal(state, std::to_string(id()).data());
  lua_getfield(state, -1, "receive");
  if (!lua_isfunction(state, -1)) {
    Support::Logger::error("MANAGED-LUA-ACTOR", "Receive is not a function");
  }
  lua_replace(state, 1);

#if CONFIG_BENCHMARK_BREAKDOWN
  if (m.get_str_attr("type") == "ping") {
    testbed_start_timekeeping(4);
  }
#endif

  PubSub::Publication* lua_pub = reinterpret_cast<PubSub::Publication*>(
      lua_newuserdata(state, sizeof(PubSub::Publication)));
  new (lua_pub) PubSub::Publication(std::move(m));
  luaL_getmetatable(state, "uActor.Publication");
  lua_setmetatable(state, -2);

#if CONFIG_BENCHMARK_BREAKDOWN
  if (lua_pub->get_str_attr("type") == "ping") {
    testbed_stop_timekeeping_inner(4, "prepare_message");
  }
#endif

  int error_code = lua_pcall(state, 1, 0, 0);
  if (error_code != 0) {
    Support::Logger::info("MANAGED-LUA-ACTOR",
                          "LUA ERROR for actor %s.%s.%s: %s", node_id(),
                          actor_type(), instance_id(), lua_tostring(state, -1));
    lua_pop(state, 1);
    return RuntimeReturnValue::RUNTIME_ERROR;
  }

  lua_gc(state, LUA_GCCOLLECT, 0);
  return RuntimeReturnValue::OK;
}

bool ManagedLuaActor::hibernate_internal() {
  lua_pushnil(state);
  lua_setglobal(state, std::to_string(id()).c_str());
  lua_gc(state, LUA_GCCOLLECT, 0);
  return true;
}

int ManagedLuaActor::publish_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  if (lua_isuserdata(state, 1) != 0 &&
      luaL_checkudata(state, 1, "uActor.Publication") != nullptr) {
    auto* pub =
        reinterpret_cast<PubSub::Publication*>(lua_touserdata(state, 1));
    actor->publish(std::move(*pub));
  } else if (lua_istable(state, -1)) {
    Support::Logger::warning("MANAGED-LUA-ACTOR",
                             "Using deprecated publication API!\n");
    actor->publish(parse_publication(actor, state, 1));
  }
  return 0;
}

int ManagedLuaActor::reply_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  if (lua_isuserdata(state, 1) != 0 &&
      luaL_checkudata(state, 1, "uActor.Publication") != nullptr) {
    auto* pub =
        reinterpret_cast<PubSub::Publication*>(lua_touserdata(state, 1));
    actor->reply(std::move(*pub));
  }

  return 0;
}

int ManagedLuaActor::republish_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  if (lua_isuserdata(state, 1) != 0 &&
      luaL_checkudata(state, 1, "uActor.Publication") != nullptr) {
    auto* pub =
        reinterpret_cast<PubSub::Publication*>(lua_touserdata(state, 1));
    actor->republish(PubSub::Publication(*pub));
  }
  return 0;
}

int ManagedLuaActor::delayed_publish_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  uint32_t delay = lua_tointeger(state, 2);

  if (lua_isuserdata(state, 1) != 0 &&
      luaL_checkudata(state, 1, "uActor.Publication") != nullptr) {
    auto* pub =
        reinterpret_cast<PubSub::Publication*>(lua_touserdata(state, 1));
    pub->set_attr(std::string_view("publisher_node_id"),
                  std::string(actor->node_id()));
    pub->set_attr(std::string_view("publisher_instance_id"),
                  std::string(actor->instance_id()));
    pub->set_attr(std::string_view("publisher_actor_type"),
                  std::string(actor->actor_type()));
    actor->delayed_publish(std::move(*pub), delay);
  }
  if (lua_istable(state, 1)) {
    Support::Logger::warning("MANAGED-LUA-ACTOR",
                             "Using deprecated publication API!\n");
    actor->delayed_publish(parse_publication(actor, state, 1), delay);
  }
  return 0;
}

int ManagedLuaActor::deferred_block_for_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  uint32_t timeout = lua_tointeger(state, 2);
  actor->deffered_block_for(std::move(parse_filters(state, 1)), timeout);
  return 0;
}

int ManagedLuaActor::subscribe_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  uint8_t priority = 0;
  if (lua_gettop(state) > 1) {
    priority = lua_tointeger(state, 2);
  }

  uint32_t id = actor->subscribe(parse_filters(state, 1), priority);
  lua_pushinteger(state, id);
  return 1;
}

int ManagedLuaActor::unsubscribe_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  uint32_t sub_id = lua_tointeger(state, 1);
  actor->unsubscribe(sub_id);
  return 0;
}

int ManagedLuaActor::now_wrapper(lua_State* state) {
  lua_pushinteger(state, BoardFunctions::timestamp());
  return 1;
}

int ManagedLuaActor::encode_base64(lua_State* state) {
  lua_len(state, 1);
  size_t size = lua_tointeger(state, -1);
  lua_pop(state, 1);
  std::string raw(lua_tolstring(state, 1, &size), size);
  lua_pop(state, 1);
  lua_pushstring(state, base64_encode(raw, false).c_str());
  return 1;
}

int ManagedLuaActor::decode_base64(lua_State* state) {
  lua_len(state, 1);
  size_t size = lua_tointeger(state, -1);
  lua_pop(state, 1);
  std::string encoded(lua_tolstring(state, 1, &size), size);
  lua_pop(state, 1);
  auto decoded = base64_decode(encoded);
  lua_pushlstring(state, decoded.c_str(), decoded.length());
  return 1;
}

int ManagedLuaActor::unix_timestamp_wrapper(lua_State* state) {
  static int MIN_ACCEPTED_TIMESTAMP = 1577836800;

  auto timestamp = std::chrono::system_clock::now().time_since_epoch();
  auto upper = std::chrono::floor<std::chrono::seconds>(timestamp);
  auto lower = timestamp - upper;

  uint32_t seconds = upper.count();
  uint32_t nanoseconds = lower.count();
  if (seconds < MIN_ACCEPTED_TIMESTAMP) {
    Support::Logger::warning("MANAGED-LUA-ACTOR",
                             "Tried to fetch outdated UNIX timestamp\n");
    lua_pushinteger(state, 0);
    lua_pushinteger(state, 0);
  } else {
    lua_pushinteger(state, seconds);
    lua_pushinteger(state, nanoseconds);
  }
  return 2;
}

int ManagedLuaActor::calculate_time_diff(lua_State* state) {
  auto timestamp = std::chrono::system_clock::now().time_since_epoch();

  uint32_t seconds = lua_tointeger(state, 1);
  uint32_t nanoseconds = lua_tointeger(state, 2);

  auto start =
      std::chrono::seconds(seconds) + std::chrono::nanoseconds(nanoseconds);

  lua_pushinteger(state, std::chrono::duration_cast<std::chrono::microseconds>(
                             timestamp - start)
                             .count());

  return 1;
}

int ManagedLuaActor::enqueue_wakeup_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  uint32_t delay = lua_tointeger(state, 1);
  std::string_view wakeup_id = std::string_view(lua_tostring(state, 2));

  actor->enqueue_wakeup(delay, wakeup_id);
  return 0;
}

int ManagedLuaActor::queue_size_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  lua_pushinteger(state, actor->queue_size());
  return 1;
}

int ManagedLuaActor::blake2s_wrapper(lua_State* state) {
  auto code = std::string_view(lua_tostring(state, 1));

  std::array<char, 32> out_buffer;

  blake2s(out_buffer.data(), 32, code.data(), code.length(), code.data(), 0);

  lua_pushstring(state, base64_encode(std::string_view(out_buffer.data(),
                                                       out_buffer.size()))
                            .data());

  return 1;
}

int ManagedLuaActor::log_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  static std::set<std::string_view> valid_levels = {
      "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
  auto message = lua_tostring(state, 1);
  std::string level = "INFO";
  if (lua_gettop(state) > 1) {
    level = std::string(lua_tostring(state, 2));
    std::transform(level.begin(), level.end(), level.begin(), ::toupper);
    if (valid_levels.find(level) == valid_levels.end()) {
      level = "INFO";
    }
  }
  std::string actor_identifier =
      std::string(actor->actor_type()) + "." + actor->instance_id();
  // The message is user defined, this needs to ensure safety:
  // https://owasp.org/www-community/attacks/Format_string_attack
  Support::Logger::log(level, actor_identifier, "%s", message);
  return 0;
}

// Overwrite Lua "print" to use the system's logging infrastructure
int ManagedLuaActor::print_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));
  std::string buffer = "";

  // Adapted from luaB_print function in Lua's lbaselib.c (license: MIT)
  int n = lua_gettop(state); /* number of arguments */
  lua_getglobal(state, "tostring");
  for (int i = 1; i <= n; i++) {
    const char* s;
    size_t l;
    lua_pushvalue(state, -1); /* function to be called */
    lua_pushvalue(state, i);  /* value to print */
    lua_call(state, 1, 1);
    s = lua_tolstring(state, -1, &l); /* get result */
    if (s == NULL) {
      return luaL_error(state, "'tostring' must return a string to 'print'");
    }
    if (i > 1) {
      buffer += "\t";
    }
    buffer += std::string(s, l);
    lua_pop(state, 1); /* pop result */
  }

  std::string actor_identifier =
      std::string(actor->actor_type()) + "." + actor->instance_id();
  // The message is user defined, this needs to ensure safety:
  // https://owasp.org/www-community/attacks/Format_string_attack
  Support::Logger::log("INFO", actor_identifier, "%s", buffer.c_str());
  return 0;
}

#if CONFIG_BENCHMARK_ENABLED

int ManagedLuaActor::testbed_log_integer_wrapper(lua_State* state) {
  const char* variable = lua_tostring(state, 1);
  uint32_t value = lua_tointeger(state, 2);

  testbed_log_integer(variable, value);
  return 0;
}

int ManagedLuaActor::testbed_log_double_wrapper(lua_State* state) {
  const char* variable = lua_tostring(state, 1);
  float value = lua_tonumber(state, 2);

  testbed_log_double(variable, value);
  return 0;
}

int ManagedLuaActor::testbed_log_string_wrapper(lua_State* state) {
  const char* variable = lua_tostring(state, 1);
  const char* value = lua_tostring(state, 2);

  testbed_log_string(variable, value);
  return 0;
}

#if ESP_IDF
IRAM_ATTR
#endif
int ManagedLuaActor::testbed_start_timekeeping_wrapper(lua_State* state) {
  size_t variable = lua_tointeger(state, 1);
  testbed_start_timekeeping(variable);
  return 0;
}

#if ESP_IDF
IRAM_ATTR
#endif
int ManagedLuaActor::testbed_stop_timekeeping_wrapper(lua_State* state) {
  size_t variable = lua_tointeger(state, 1);
  const char* name = lua_tostring(state, 2);
  testbed_stop_timekeeping(variable, name);
  return 0;
}

#if CONFIG_TESTBED_NESTED_TIMEKEEPING
#if ESP_IDF
IRAM_ATTR
#endif
int ManagedLuaActor::testbed_stop_timekeeping_inner_wrapper(lua_State* state) {
  size_t variable = lua_tointeger(state, 1);
  const char* name = lua_tostring(state, 2);
  testbed_stop_timekeeping_inner(variable, name);
  return 0;
}
#endif
#endif

#if CONFIG_UACTOR_ENABLE_TELEMETRY
int ManagedLuaActor::telemetry_set_wrapper(lua_State* state) {
  auto variable = std::string(lua_tostring(state, 1));
  auto value = lua_tonumber(state, 2);
  uActor::Controllers::TelemetryData::set(variable, value);
  return 0;
}
#endif

bool ManagedLuaActor::createActorEnvironment(
    std::string_view receive_function) {
  lua_createtable(state, 0, 4);  // 1

  lua_pushstring(state, node_id());
  lua_setfield(state, -2, "node_id");  // 1

  lua_pushstring(state, actor_type());
  lua_setfield(state, -2, "actor_type");  // 1

  lua_pushstring(state, instance_id());
  lua_setfield(state, -2, "instance_id");  // 1

  lua_newtable(state);                       // 2
  lua_pushlightuserdata(state, this);        // 3
  lua_pushcclosure(state, &actor_index, 1);  // 3
  lua_setfield(state, -2, "__index");        // 2
  lua_setmetatable(state, -2);               // 1

  if (luaL_loadbuffer(state, receive_function.data(), receive_function.size(),
                      actor_type())) {
    Support::Logger::warning(
        "MANAGED-LUA-ACTOR", "LUA LOAD ERROR for actor %s.%s.%s: %s", node_id(),
        actor_type(), instance_id(), lua_tostring(state, -1));
    lua_pop(state, 2);
    return false;
  }

  // Sandbox the function by settings its
  // global environment to the table created above
  lua_pushvalue(state, -2);      // 3
  lua_setupvalue(state, -2, 1);  // 2

  int error_code = lua_pcall(state, 0, 0, 0);
  if (error_code != 0) {
    Support::Logger::warning(
        "MANAGED-LUA-ACTOR", "LUA LOAD ERROR for actor %s.%s.%s: %s", node_id(),
        actor_type(), instance_id(), lua_tostring(state, -1));
    lua_pop(state, 3);
    return false;
  }

  lua_getmetatable(state, -1);
  lua_getglobal(state, (std::string("state_") + std::to_string(id())).c_str());
  lua_setfield(state, -2, "__newindex");  // 3
  lua_pop(state, 1);

  lua_getfield(state, -1, "receive");  // 2
  if (!lua_isfunction(state, -1)) {
    lua_pop(state, 3);
    return false;
  }
  lua_pop(state, 1);  // 1

  lua_setglobal(state, std::to_string(id()).data());  // 0
  lua_gc(state, LUA_GCCOLLECT, 0);
  return true;
}

int ManagedLuaActor::actor_index(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  std::string_view key = std::string_view(lua_tostring(state, 2));

  const auto* closure = closures.find(frozen::string(key));
  if (closure != closures.end()) {
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_pushcclosure(state, closure->second, 1);
    return 1;
  }

  const auto* function = LuaFunctions::base_functions.find(frozen::string(key));
  if (function != LuaFunctions::base_functions.end() &&
      function->second.first) {
    lua_pushcfunction(state, *(function->second.second));
    return 1;
  }

  for (uint32_t i = 1; i <= PubSub::ConstraintPredicates::MAX_INDEX; i++) {
    if (key == PubSub::ConstraintPredicates::name(i)) {
      lua_pushinteger(state, i);
      return 1;
    }
  }

  if (key == "math") {
    lua_getglobal(state, "math");
    return 1;
  }

  if (key == "string") {
    lua_getglobal(state, "string");
    return 1;
  }

  if (key == "table") {
    lua_getglobal(state, "table");
    return 1;
  }

  if (key == "Publication") {
    lua_getglobal(state, "Publication");
    return 1;
  }

  lua_getglobal(
      state,
      (std::string("state_") + std::to_string(actor->id())).c_str());  // 2
  lua_getfield(state, -1, key.data());
  return 1;

  lua_pushnil(state);
  return 1;
}

ManagedLuaActor::RuntimeReturnValue ManagedLuaActor::fetch_code_and_init() {
  auto result = CodeStore::get_instance().retrieve(
      CodeIdentifier(actor_type(), actor_version(), std::string_view("lua")));
  if (result) {
    if (createActorEnvironment(result->code)) {
      return RuntimeReturnValue::OK;
    } else {
      return RuntimeReturnValue::INITIALIZATION_ERROR;
    }
  } else {
    trigger_code_fetch();
    return RuntimeReturnValue::NOT_READY;
  }
}

PubSub::Filter ManagedLuaActor::parse_filters(lua_State* state, size_t index) {
  std::list<PubSub::Constraint> filter_list;
  luaL_checktype(state, index, LUA_TTABLE);
  lua_pushvalue(state, index);
  lua_pushnil(state);

  while (lua_next(state, -2) != 0) {
    lua_pushvalue(state, -2);
    std::string key(lua_tostring(state, -1));

    // Simple Equality
    if (lua_type(state, -2) == LUA_TSTRING) {
      std::string value(lua_tostring(state, -2));
      filter_list.emplace_back(std::move(key), std::move(value));
    } else if (lua_isinteger(state, -2) != 0) {
      int32_t value = lua_tointeger(state, -2);
      filter_list.emplace_back(std::move(key), value);
    } else if (lua_isnumber(state, -2) != 0) {
      float value = lua_tonumber(state, -2);
      filter_list.emplace_back(std::move(key), value);
    } else if (lua_type(state, -2) == LUA_TTABLE) {  //  Complex Operator
      lua_geti(state, -2, 1);
      PubSub::ConstraintPredicates::Predicate operation =
          PubSub::ConstraintPredicates::Predicate::EQ;
      int32_t value = lua_tointeger(state, -1);
      if (lua_isinteger(state, -1) != 0 && value > 0 &&
          value <= PubSub::ConstraintPredicates::MAX_INDEX) {
        operation = static_cast<PubSub::ConstraintPredicates::Predicate>(
            lua_tointeger(state, -1));
      } else {
        luaL_error(state, "Bad Predicate");
      }
      lua_pop(state, 1);

      bool optional = false;
      lua_getfield(state, -2, "optional");
      if (lua_isboolean(state, -1)) {
        optional = (lua_toboolean(state, -1) != 0);
      }
      lua_pop(state, 1);

      lua_geti(state, -2, 2);
      if (lua_isinteger(state, -1) != 0) {
        filter_list.emplace_back(std::move(key),
                                 static_cast<int32_t>(lua_tointeger(state, -1)),
                                 operation, optional);
      } else if (lua_isnumber(state, -1) != 0) {
        filter_list.emplace_back(std::move(key),
                                 static_cast<float>(lua_tonumber(state, -1)),
                                 operation, optional);
      } else if (lua_isstring(state, -1) != 0) {
        filter_list.emplace_back(std::move(key),
                                 std::string_view(lua_tostring(state, -1)),
                                 operation, optional);
      }
      lua_pop(state, 1);
    }
    lua_pop(state, 2);
  }
  lua_pop(state, 1);

  return PubSub::Filter(std::move(filter_list));
}

PubSub::Publication ManagedLuaActor::parse_publication(ManagedLuaActor* actor,
                                                       lua_State* state,
                                                       size_t index) {
#if CONFIG_BENCHMARK_BREAKDOWN
  testbed_start_timekeeping(5);
#endif
  auto p = PubSub::Publication(actor->node_id(), actor->actor_type(),
                               actor->instance_id());

  luaL_checktype(state, index, LUA_TTABLE);
  lua_pushvalue(state, index);
  lua_pushnil(state);

  while (lua_next(state, -2) != 0) {
    lua_pushvalue(state, -2);
    std::string key(lua_tostring(state, -1));

    if (lua_type(state, -2) == LUA_TSTRING) {
      std::string value(lua_tostring(state, -2));
      p.set_attr(std::move(key), std::move(value));
    } else if (lua_isinteger(state, -2) != 0) {
      int32_t value = lua_tointeger(state, -2);
      p.set_attr(std::move(key), value);
    } else if (lua_isnumber(state, -2) != 0) {
      float value = lua_tonumber(state, -2);
      p.set_attr(std::move(key), value);
    }
    lua_pop(state, 2);
  }
  lua_pop(state, 1);
#if CONFIG_BENCHMARK_BREAKDOWN
  if (p.get_str_attr("type") == "ping") {
    testbed_stop_timekeeping_inner(5, "parse_publication");
  }
#endif
  return std::move(p);
}

}  // namespace uActor::ActorRuntime
