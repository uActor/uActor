#ifndef MAIN_INCLUDE_MANAGED_LUA_ACTOR_HPP_
#define MAIN_INCLUDE_MANAGED_LUA_ACTOR_HPP_

#include <cstdio>
#include <list>
#include <string>
#include <utility>

#include "lua.hpp"
#include "managed_actor.hpp"
#include "publication.hpp"
#include "subscription.hpp"

class ManagedLuaActor : public ManagedActor {
 public:
  ManagedLuaActor(RuntimeApi* api, uint32_t unique_id, const char* node_id,
                  const char* actor_type, const char* instance_id,
                  const char* code, lua_State* global_state)
      : ManagedActor(api, unique_id, node_id, actor_type, instance_id, code),
        state(global_state) {}

  ~ManagedLuaActor() {
    if (initialized()) {
      lua_pushglobaltable(state);
      lua_pushnil(state);
      lua_seti(state, -2, id());
      lua_pop(state, 1);
      lua_gc(state, LUA_GCCOLLECT, 0);
    }
  }

  bool receive(const Publication& m) {
    if (!initialized()) {
      printf("Actor not initialized, can't process message\n");
      return false;
    }

    lua_pushglobaltable(state);
    lua_geti(state, -1, id());
    lua_getfield(state, -1, "receive");
    if (!lua_isfunction(state, -1)) {
      printf("receive is not a function\n");
    }
    lua_replace(state, 1);
    lua_pop(state, 1);

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

    int error_code = lua_pcall(state, 1, 0, 0);
    if (error_code) {
      printf("LUA ERROR for actor %s.%s.%s\n", node_id(), actor_type(),
             instance_id());
      if (error_code == LUA_ERRRUN) {
        printf("ERROR: %s\n", lua_tostring(state, -1));
        lua_pop(state, 1);
      }
      return false;
    }

    lua_gc(state, LUA_GCCOLLECT, 0);
    return true;
  }

 protected:
  bool internal_initialize() { return createActorEnvironment(code()); }

 private:
  lua_State* state;

  static int publish_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));

    actor->publish(parse_publication(actor, state, 1));
    return 0;
  }

  static int delayed_publish_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));

    uint32_t delay = lua_tointeger(state, 2);

    actor->delayed_publish(parse_publication(actor, state, 1), delay);
    return 0;
  }

  static int deferred_block_for_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));

    uint32_t timeout = lua_tointeger(state, 2);
    actor->deffered_block_for(std::move(parse_filters(state, 1)), timeout);
    return 0;
  }

  static int subscribe_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));

    uint32_t id = actor->subscribe(parse_filters(state, 1));
    lua_pushinteger(state, id);
    return 1;
  }

  static int unsubscribe_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));

    uint32_t sub_id = lua_tointeger(state, 1);
    actor->unsubscribe(sub_id);
    return 0;
  }

  static constexpr luaL_Reg core[] = {
      {"publish", &publish_wrapper},
      {"delayed_publish", &delayed_publish_wrapper},
      {"deferred_block_for", &deferred_block_for_wrapper},
      {"subscribe", &subscribe_wrapper},
      {"unsubscribe", &unsubscribe_wrapper},
      {NULL, NULL}};

  bool createActorEnvironment(const char* receive_function) {
    lua_pushglobaltable(state);          // 1
    luaL_newlibtable(state, core);       // 2
    lua_pushlightuserdata(state, this);  // 3
    luaL_setfuncs(state, core, 1);       // 2

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

    for (uint32_t i = 1; i <= PubSub::ConstraintPredicates::MAX_INDEX; i++) {
      const char* name = PubSub::ConstraintPredicates::name(i);
      if (name) {
        lua_pushinteger(state, i);
        lua_setfield(state, -2, name);
      }
    }

    lua_newtable(state);               // 3
    lua_pushvalue(state, -1);          // 4
    lua_setfield(state, -3, "state");  // 3

    lua_newtable(state);                    // 4
    lua_pushvalue(state, -2);               // 5
    lua_setfield(state, -2, "__index");     // 4
    lua_pushvalue(state, -2);               // 5
    lua_setfield(state, -2, "__newindex");  // 4
    lua_setmetatable(state, -3);            // 3
    lua_pop(state, 1);                      // 2

    if (luaL_loadstring(state, receive_function)) {
      printf("LUA LOAD ERROR for actor %s.%s.%s\n", node_id(), actor_type(),
             instance_id());
      printf("ERROR: %s\n", lua_tostring(state, -1));
      lua_pop(state, 3);
      return false;
    }

    // Sandbox the function by settings its
    // global environment to the table created above
    lua_pushvalue(state, -2);      // 4
    lua_setupvalue(state, -2, 1);  // 3

    int error_code = lua_pcall(state, 0, 0, 0);  // 2 - 3
    if (error_code) {
      printf("LUA LOAD ERROR for actor %s.%s.%s\n", node_id(), actor_type(),
             instance_id());
      if (error_code == LUA_ERRRUN) {
        printf("ERROR: %s\n", lua_tostring(state, -1));
        lua_pop(state, 3);
      } else {
        lua_pop(state, 2);
      }
      return false;
    }

    lua_getfield(state, -1, "receive");  // 3
    if (!lua_isfunction(state, -1)) {
      lua_pop(state, 3);  // 0
      return false;
    }
    lua_pop(state, 1);  // 2

    lua_seti(state, -2, id());  // 1
    lua_pop(state, 1);
    return true;
  }

  static PubSub::Filter parse_filters(lua_State* state, size_t index) {
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
          filter_list.emplace_back(
              std::move(key), static_cast<int32_t>(lua_tointeger(state, -1)),
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

  static Publication parse_publication(ManagedLuaActor* actor, lua_State* state,
                                       size_t index) {
    Publication p = Publication(actor->node_id(), actor->actor_type(),
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
    return std::move(p);
  }
};

#endif  // MAIN_INCLUDE_MANAGED_LUA_ACTOR_HPP_
