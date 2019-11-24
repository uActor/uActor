#ifndef MAIN_INCLUDE_LUA_RUNTIME_HPP_
#define MAIN_INCLUDE_LUA_RUNTIME_HPP_

#include <lua.hpp>
#include <utility>

#include "actor_runtime.hpp"
#include "managed_actor.hpp"
#include "managed_lua_actor.hpp"

class LuaRuntime : public ActorRuntime<ManagedLuaActor, LuaRuntime> {
 public:
  LuaRuntime(RouterV2* router, char* id)
      : ActorRuntime<ManagedLuaActor, LuaRuntime>(router, id) {
    state = create_lua_state();
  }

 private:
  lua_State* state;

  lua_State* create_lua_state() {
    lua_State* lua_state = luaL_newstate();
    luaL_requiref(lua_state, "_G", luaopen_base, 1);
    lua_pop(lua_state, 1);
    return lua_state;
  }

  void add_actor(const char* payload) {
    add_actor_base<lua_State*>(payload, state);
  }

  friend ActorRuntime;
};

#endif  // MAIN_INCLUDE_LUA_RUNTIME_HPP_