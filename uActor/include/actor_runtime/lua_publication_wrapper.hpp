#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_PUBLICATION_WRAPPER_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_PUBLICATION_WRAPPER_HPP_

#include <lua.hpp>

#include "pubsub/publication.hpp"

namespace uActor::ActorRuntime {
struct LuaPublicationWrapper {
  static int get(lua_State* state);

  static int gc(lua_State* state);

  static int initialize(lua_State* state);

  static int set(lua_State* state);

  static int luaopen(lua_State* state);
};
}  // namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_PUBLICATION_WRAPPER_HPP_
