#pragma once

#include <lua.hpp>
#include <memory>

#include "pubsub/publication.hpp"

namespace uActor::ActorRuntime {

// There is a large overlap between Publication and Publication::Map (they could
// implement the same interface).
struct LuaPublicationBaseWrapper {
  static int get_from_map(lua_State* state,
                          std::shared_ptr<PubSub::Publication::Map> map_handle);

  static int set_in_map(lua_State* state,
                        std::shared_ptr<PubSub::Publication::Map> map_handle);

  static void parse_table(std::shared_ptr<PubSub::Publication::Map> component,
                          lua_State* lua_state, int32_t index);
};

struct LuaPublicationWrapper : LuaPublicationBaseWrapper {
  static int get(lua_State* state);

  static int gc(lua_State* state);

  static int initialize(lua_State* state);

  static int set(lua_State* state);

  static int to_string(lua_State* state);

  static int luaopen(lua_State* state);
};

// Wrap the shared pointer
struct LuaPublicationMapWrapper : LuaPublicationBaseWrapper {
  LuaPublicationMapWrapper() = delete;

  explicit LuaPublicationMapWrapper(
      std::shared_ptr<PubSub::Publication::Map> data)
      : data(data) {}

  ~LuaPublicationMapWrapper() = default;

  std::shared_ptr<PubSub::Publication::Map> data;

  static int get(lua_State* state);

  static int gc(lua_State* state);

  static int set(lua_State* state);

  static int luaopen(lua_State* state);

  static int to_string(lua_State* state);
};

}  // namespace uActor::ActorRuntime
