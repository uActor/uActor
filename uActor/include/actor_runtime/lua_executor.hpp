#pragma once

#include <frozen/string.h>
#include <frozen/unordered_map.h>

#include <lua.hpp>
#include <string_view>

#include "executor.hpp"
#include "managed_lua_actor.hpp"

namespace uActor::ActorRuntime {

class LuaExecutor : public Executor<ManagedLuaActor, LuaExecutor> {
 public:
  LuaExecutor(uActor::PubSub::Router* router, const char* node_id,
              const char* id);

  ~LuaExecutor() override;

 private:
  lua_State* state;

  lua_State* create_lua_state();

  void luaopen_uactor_publication(lua_State* state);

  void add_actor(uActor::PubSub::Publication&& publication) {
    add_actor_base<lua_State*>(publication, state);
  }

  void open_lua_base_optimized(lua_State* state);
  void open_lua_math_optimized(lua_State* state);
  void open_lua_string_optimized(lua_State* state);
  void open_lua_table_optimized(lua_State* state);
  void add_constraint_predicate_table(lua_State* lua_state);

  static int base_index(lua_State* state);
  static int math_index(lua_State* state);
  static int string_index(lua_State* state);
  static int table_index(lua_State* state);
  static int constraint_predicate_index(lua_State* lua_state);

  friend Executor;
};

}  //  namespace uActor::ActorRuntime
