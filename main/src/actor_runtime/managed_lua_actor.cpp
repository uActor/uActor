#include "actor_runtime/managed_lua_actor.hpp"

#include <cassert>

namespace uActor::ActorRuntime {

ManagedLuaActor::~ManagedLuaActor() {
  lua_pushnil(state);
  lua_setglobal(state, std::to_string(id()).data());
  lua_gc(state, LUA_GCCOLLECT, 0);
}

bool ManagedLuaActor::receive(const PubSub::Publication& m) {
#if CONFIG_BENCHMARK_BREAKDOWN
  if (m.get_str_attr("type") == "ping") {
    testbed_stop_timekeeping_inner(6, "scheduling");
  }
#endif
  if (!initialized()) {
    printf("Actor not initialized, can't process message\n");
    return false;
  }

  lua_getglobal(state, std::to_string(id()).data());
  lua_getfield(state, -1, "receive");
  if (!lua_isfunction(state, -1)) {
    printf("receive is not a function\n");
  }
  lua_replace(state, 1);

#if CONFIG_BENCHMARK_BREAKDOWN
  if (m.get_str_attr("type") == "ping") {
    testbed_start_timekeeping(4);
  }
#endif
  lua_newtable(state);
  for (const auto& value : m) {
    if (std::holds_alternative<std::string>(value.second)) {
      lua_pushstring(state, std::get<std::string>(value.second).c_str());
    } else if (std::holds_alternative<int32_t>(value.second)) {
      lua_pushinteger(state, std::get<int32_t>(value.second));
    } else if (std::holds_alternative<float>(value.second)) {
      lua_pushnumber(state, std::get<float>(value.second));
    }
    lua_setfield(state, -2, value.first.c_str());
  }
#if CONFIG_BENCHMARK_BREAKDOWN
  if (m.get_str_attr("type") == "ping") {
    testbed_stop_timekeeping_inner(4, "prepare_message");
  }
#endif

  int error_code = lua_pcall(state, 1, 0, 0);
  if (error_code) {
    printf("LUA ERROR for actor %s.%s.%s\n", node_id(), actor_type(),
           instance_id());
    printf("ERROR: %s\n", lua_tostring(state, -1));
    lua_pop(state, 1);
    return false;
  }

  lua_gc(state, LUA_GCCOLLECT, 0);

  return true;
}

int ManagedLuaActor::publish_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  actor->publish(parse_publication(actor, state, 1));
  return 0;
}

int ManagedLuaActor::delayed_publish_wrapper(lua_State* state) {
  ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
      lua_touserdata(state, lua_upvalueindex(1)));

  uint32_t delay = lua_tointeger(state, 2);

  actor->delayed_publish(parse_publication(actor, state, 1), delay);
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

  uint32_t id = actor->subscribe(parse_filters(state, 1));
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

int ManagedLuaActor::testbed_start_timekeeping_wrapper(lua_State* state) {
  size_t variable = lua_tointeger(state, 1);
  testbed_start_timekeeping(variable);
  return 0;
}

int ManagedLuaActor::testbed_stop_timekeeping_wrapper(lua_State* state) {
  size_t variable = lua_tointeger(state, 1);
  const char* name = lua_tostring(state, 2);
  testbed_stop_timekeeping(variable, name);
  return 0;
}

int ManagedLuaActor::testbed_stop_timekeeping_inner_wrapper(lua_State* state) {
  size_t variable = lua_tointeger(state, 1);
  const char* name = lua_tostring(state, 2);
  testbed_stop_timekeeping_inner(variable, name);
  return 0;
}
#endif

bool ManagedLuaActor::createActorEnvironment(const char* receive_function) {
  lua_createtable(state, 0, 16);        // 1
  lua_pushlightuserdata(state, this);   // 2
  luaL_setfuncs(state, actor_core, 1);  // 1

  lua_pushstring(state, node_id());
  lua_setfield(state, -2, "node_id");

  lua_pushstring(state, actor_type());
  lua_setfield(state, -2, "actor_type");

  lua_pushstring(state, instance_id());
  lua_setfield(state, -2, "instance_id");

  lua_getglobal(state, "print");
  lua_setfield(state, -2, "print");

  lua_getglobal(state, "tostring");
  lua_setfield(state, -2, "tostring");

  lua_getglobal(state, "tonumber");
  lua_setfield(state, -2, "tonumber");

  // Benchmarking
  lua_getglobal(state, "collectgarbage");
  lua_setfield(state, -2, "collectgarbage");

  lua_getglobal(state, "math");
  assert(lua_istable(state, -1));
  lua_setfield(state, -2, "math");

  for (uint32_t i = 1; i <= PubSub::ConstraintPredicates::MAX_INDEX; i++) {
    const char* name = PubSub::ConstraintPredicates::name(i);
    if (name) {
      lua_pushinteger(state, i);
      lua_setfield(state, -2, name);
    }
  }

  lua_newtable(state);               // 2
  lua_pushvalue(state, -1);          // 3
  lua_setfield(state, -3, "state");  // 2

  lua_newtable(state);                    // 3
  lua_pushvalue(state, -2);               // 4
  lua_setfield(state, -2, "__index");     // 3
  lua_pushvalue(state, -2);               // 4
  lua_setfield(state, -2, "__newindex");  // 3
  lua_setmetatable(state, -3);            // 2
  lua_pop(state, 1);                      // 1

  if (luaL_loadstring(state, receive_function)) {
    printf("LUA LOAD ERROR for actor %s.%s.%s\n", node_id(), actor_type(),
           instance_id());
    printf("ERROR: %s\n", lua_tostring(state, -1));
    lua_pop(state, 2);
    return false;
  }

  // Sandbox the function by settings its
  // global environment to the table created above
  lua_pushvalue(state, -2);      // 3
  lua_setupvalue(state, -2, 1);  // 2

  int error_code = lua_pcall(state, 0, 0, 0);
  if (error_code) {
    printf("LUA LOAD ERROR for actor %s.%s.%s\n", node_id(), actor_type(),
           instance_id());
    printf("ERROR: %s\n", lua_tostring(state, -1));
    lua_pop(state, 3);
    return false;
  }

  lua_getfield(state, -1, "receive");  // 2
  if (!lua_isfunction(state, -1)) {
    lua_pop(state, 3);
    return false;
  }
  lua_pop(state, 1);  // 1

  lua_setglobal(state, std::to_string(id()).data());  // 0
  return true;
}

PubSub::Filter ManagedLuaActor::parse_filters(lua_State* state, size_t index) {
  std::list<PubSub::Constraint> filter_list;
  luaL_checktype(state, index, LUA_TTABLE);
  lua_pushvalue(state, index);
  lua_pushnil(state);

  while (lua_next(state, -2)) {
    lua_pushvalue(state, -2);
    std::string key(lua_tostring(state, -1));

    // Simple Equality
    if (lua_type(state, -2) == LUA_TSTRING) {
      std::string value(lua_tostring(state, -2));
      filter_list.emplace_back(std::move(key), std::move(value));
    } else if (lua_isinteger(state, -2)) {
      int32_t value = lua_tointeger(state, -2);
      filter_list.emplace_back(std::move(key), value);
    } else if (lua_isnumber(state, -2)) {
      float value = lua_tonumber(state, -2);
      filter_list.emplace_back(std::move(key), value);
    } else if (lua_type(state, -2) == LUA_TTABLE) {  //  Complex Operator
      lua_geti(state, -2, 1);
      PubSub::ConstraintPredicates::Predicate operation =
          PubSub::ConstraintPredicates::Predicate::EQ;
      int32_t value = lua_tointeger(state, -1);
      if (lua_isinteger(state, -1) && value > 0 &&
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
        optional = lua_toboolean(state, -1);
      }
      lua_pop(state, 1);

      lua_geti(state, -2, 2);
      if (lua_isinteger(state, -1)) {
        filter_list.emplace_back(std::move(key),
                                 static_cast<int32_t>(lua_tointeger(state, -1)),
                                 operation, optional);
      } else if (lua_isnumber(state, -1)) {
        filter_list.emplace_back(std::move(key),
                                 static_cast<float>(lua_tonumber(state, -1)),
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

  while (lua_next(state, -2)) {
    lua_pushvalue(state, -2);
    std::string key(lua_tostring(state, -1));

    if (lua_type(state, -2) == LUA_TSTRING) {
      std::string value(lua_tostring(state, -2));
      p.set_attr(std::move(key), std::move(value));
    } else if (lua_isinteger(state, -2)) {
      int32_t value = lua_tointeger(state, -2);
      p.set_attr(std::move(key), value);
    } else if (lua_isnumber(state, -2)) {
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

luaL_Reg ManagedLuaActor::actor_core[] = {
    {"publish", &publish_wrapper},
    {"delayed_publish", &delayed_publish_wrapper},
    {"deferred_block_for", &deferred_block_for_wrapper},
    {"subscribe", &subscribe_wrapper},
    {"unsubscribe", &unsubscribe_wrapper},
    {"now", &now_wrapper},
#if CONFIG_BENCHMARK_ENABLED
    {"testbed_log_integer", &testbed_log_integer_wrapper},
    {"testbed_log_double", &testbed_log_double_wrapper},
    {"testbed_log_string", &testbed_log_string_wrapper},
    {"testbed_start_timekeeping", &testbed_start_timekeeping_wrapper},
    {"testbed_stop_timekeeping", &testbed_stop_timekeeping_wrapper},
    {"testbed_stop_timekeeping_inner", &testbed_stop_timekeeping_inner_wrapper},
#endif
    {NULL, NULL}};

}  // namespace uActor::ActorRuntime
