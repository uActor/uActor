// adapted from lua-5.3.5.tar.gz
#ifndef COMPONENTS_LUA_INCLUDE_LUA_HPP_
#define COMPONENTS_LUA_INCLUDE_LUA_HPP_

#include <climits>
#include <cstddef>

// lua.hpp
// Lua header files for C++
// <<extern "C">> not supplied automatically because Lua also compiles as C++
extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}
#endif  // COMPONENTS_LUA_INCLUDE_LUA_HPP_
