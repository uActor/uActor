#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_EXECUTOR_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_EXECUTOR_HPP_

#include <lua.hpp>

#include "executor.hpp"
#include "managed_lua_actor.hpp"

namespace uActor::ActorRuntime {

class LuaExecutor : public Executor<ManagedLuaActor, LuaExecutor> {
 public:
  LuaExecutor(uActor::PubSub::Router* router, const char* node_id,
              const char* id);

  ~LuaExecutor();

 private:
  lua_State* state;

  lua_State* create_lua_state();

  void luaopen_uactor_publication(lua_State* state);

  void add_actor(uActor::PubSub::Publication&& publication) {
    add_actor_base<lua_State*>(publication, state);
  }

  friend Executor;
};

}  //  namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_EXECUTOR_HPP_
