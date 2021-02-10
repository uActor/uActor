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

  static int base_index(lua_State* state);
  static int math_index(lua_State* state);
  static int string_index(lua_State* state);

  static lua_CFunction math_function_store[23];
  constexpr static frozen::unordered_map<frozen::string, lua_CFunction*, 23>
      math_functions = {{"abs", math_function_store},
                        {"acos", math_function_store + 1},
                        {"asin", math_function_store + 2},
                        {"atan", math_function_store + 3},
                        {"ceil", math_function_store + 4},
                        {"cos", math_function_store + 5},
                        {"deg", math_function_store + 6},
                        {"exp", math_function_store + 7},
                        {"tointeger", math_function_store + 8},
                        {"floor", math_function_store + 9},
                        {"fmod", math_function_store + 10},
                        {"ult", math_function_store + 11},
                        {"log", math_function_store + 12},
                        {"max", math_function_store + 13},
                        {"min", math_function_store + 14},
                        {"modf", math_function_store + 15},
                        {"rad", math_function_store + 16},
                        {"random", math_function_store + 17},
                        {"randomseed", math_function_store + 18},
                        {"sin", math_function_store + 19},
                        {"sqrt", math_function_store + 20},
                        {"tan", math_function_store + 21},
                        {"type", math_function_store + 22}};

  static lua_CFunction string_function_store[23];
  constexpr static frozen::unordered_map<frozen::string, lua_CFunction*, 17>
      string_functions = {
          {"byte", string_function_store},
          {"char", string_function_store + 1},
          {"dump", string_function_store + 2},
          {"find", string_function_store + 3},
          {"format", string_function_store + 4},
          {"gmatch", string_function_store + 5},
          {"gsub", string_function_store + 6},
          {"len", string_function_store + 7},
          {"lower", string_function_store + 8},
          {"match", string_function_store + 9},
          {"rep", string_function_store + 10},
          {"reverse", string_function_store + 11},
          {"sub", string_function_store + 12},
          {"upper", string_function_store + 13},
          {"pack", string_function_store + 14},
          {"packsize", string_function_store + 15},
          {"unpack", string_function_store + 16},
      };

  friend Executor;
};

}  //  namespace uActor::ActorRuntime
