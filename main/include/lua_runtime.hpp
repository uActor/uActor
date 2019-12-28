#ifndef MAIN_INCLUDE_LUA_RUNTIME_HPP_
#define MAIN_INCLUDE_LUA_RUNTIME_HPP_

#include <lua.hpp>
#include <utility>

#include "actor_runtime.hpp"
#include "managed_actor.hpp"
#include "managed_lua_actor.hpp"

class LuaRuntime : public ActorRuntime<ManagedLuaActor, LuaRuntime> {
 public:
  LuaRuntime(RouterV3* router, const char* node_id, const char* id)
      : ActorRuntime<ManagedLuaActor, LuaRuntime>(router, node_id,
                                                  "lua_runtime", id) {
    state = create_lua_state();
  }

  ~LuaRuntime() {
    actors.clear();
    lua_close(state);
  }

 private:
  lua_State* state;

  lua_State* create_lua_state() {
    lua_State* lua_state = luaL_newstate();
    luaL_requiref(lua_state, "_G", luaopen_base, 1);
    lua_pop(lua_state, 1);
    return lua_state;
  }

  void add_actor(Publication&& publication) {
    add_actor_base<lua_State*>(publication, state);
  }

  friend ActorRuntime;
};

#endif  // MAIN_INCLUDE_LUA_RUNTIME_HPP_
