#pragma once

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
