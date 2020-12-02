#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_FUNCTIONS_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_FUNCTIONS_HPP_

#include <frozen/string.h>
#include <frozen/unordered_map.h>

#include <lua.hpp>
#include <utility>

namespace uActor::ActorRuntime {

struct LuaFunctions {
  static lua_CFunction base_function_store[];

  constexpr static frozen::unordered_map<frozen::string,
                                         std::pair<bool, lua_CFunction*>, 22>
      base_functions = {{"assert", {true, base_function_store}},
                        {"collectgarbage", {true, base_function_store + 1}},
                        {"error", {true, base_function_store + 2}},
                        {"ipairs", {true, base_function_store + 3}},
                        {"next", {true, base_function_store + 4}},
                        {"pairs", {true, base_function_store + 5}},
                        {"print", {true, base_function_store + 6}},
                        {"select", {true, base_function_store + 7}},
                        {"tonumber", {true, base_function_store + 8}},
                        {"tostring", {true, base_function_store + 9}},
                        {"type", {true, base_function_store + 10}},
                        //
                        {"dofile", {false, base_function_store + 11}},
                        {"getmetatable", {false, base_function_store + 12}},
                        {"loadfile", {false, base_function_store + 13}},
                        {"load", {false, base_function_store + 14}},
                        {"pcall", {false, base_function_store + 15}},
                        {"rawequal", {false, base_function_store + 16}},
                        {"rawlen", {false, base_function_store + 17}},
                        {"rawget", {false, base_function_store + 18}},
                        {"rawset", {false, base_function_store + 19}},
                        {"setmetatable", {false, base_function_store + 20}},
                        {"xpcall", {false, base_function_store + 21}}};
};
}  // namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_LUA_FUNCTIONS_HPP_
