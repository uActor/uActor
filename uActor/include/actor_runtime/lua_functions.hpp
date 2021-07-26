#pragma once

#include <frozen/string.h>
#include <frozen/unordered_map.h>

#include <lua.hpp>
#include <utility>

namespace uActor::ActorRuntime {

// Read-only index maps for the functions shipping with the Lua distribution.
// The functions are currently loaded in the LuaExecutor. Future versions of the
// system might patch the Lua codebase to allow populating these directly.
struct LuaFunctions {
  // We need to export all base functions as they might be used by the
  // iniatialization code of certain libraries. However, only a subset
  // (bool=true) can be exposed to the actors.
  static lua_CFunction base_function_store[22];
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

  static lua_CFunction table_function_store[7];
  constexpr static frozen::unordered_map<frozen::string, lua_CFunction*, 7>
      table_functions = {{"concat", table_function_store},
                         {"insert", table_function_store + 1},
                         {"move", table_function_store + 2},
                         {"pack", table_function_store + 3},
                         {"remove", table_function_store + 4},
                         {"sort", table_function_store + 5},
                         {"unpack", table_function_store + 6}};
};
}  // namespace uActor::ActorRuntime
