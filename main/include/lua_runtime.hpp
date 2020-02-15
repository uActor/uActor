#ifndef MAIN_INCLUDE_LUA_RUNTIME_HPP_
#define MAIN_INCLUDE_LUA_RUNTIME_HPP_

#include <lua.hpp>
#include <utility>

#include "actor_runtime.hpp"
#include "managed_actor.hpp"
#include "managed_lua_actor.hpp"

class LuaRuntime : public ActorRuntime<ManagedLuaActor, LuaRuntime> {
 public:
  LuaRuntime(uActor::PubSub::Router* router, const char* node_id,
             const char* id)
      : ActorRuntime<ManagedLuaActor, LuaRuntime>(router, node_id,
                                                  "lua_runtime", id) {
    state = create_lua_state();

    uActor::PubSub::Publication p{BoardFunctions::NODE_ID, "lua_runtime", "1"};
    p.set_attr("type", "runtime_update");
    p.set_attr("command", "register");
    p.set_attr("actor_runtime_type", "lua");
    p.set_attr("update_node_id", BoardFunctions::NODE_ID);
    p.set_attr("update_actor_type", "lua_runtime");
    p.set_attr("update_instance_id", "1");
    p.set_attr("node_id", BoardFunctions::NODE_ID);
    uActor::PubSub::Router::get_instance().publish(std::move(p));
  }

  ~LuaRuntime() {
    uActor::PubSub::Publication p{BoardFunctions::NODE_ID, "lua_runtime", "1"};
    p.set_attr("type", "runtime_update");
    p.set_attr("command", "deregister");
    p.set_attr("actor_runtime_type", "lua");
    p.set_attr("update_node_id", BoardFunctions::NODE_ID);
    p.set_attr("update_actor_type", "lua_runtime");
    p.set_attr("update_instance_id", "1");
    p.set_attr("node_id", BoardFunctions::NODE_ID);
    uActor::PubSub::Router::get_instance().publish(std::move(p));

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

  void add_actor(uActor::PubSub::Publication&& publication) {
    add_actor_base<lua_State*>(publication, state);
  }

  friend ActorRuntime;
};

#endif  // MAIN_INCLUDE_LUA_RUNTIME_HPP_
