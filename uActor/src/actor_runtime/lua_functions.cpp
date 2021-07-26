#include "actor_runtime/lua_functions.hpp"

namespace uActor::ActorRuntime {
lua_CFunction LuaFunctions::base_function_store[];
lua_CFunction LuaFunctions::math_function_store[];
lua_CFunction LuaFunctions::string_function_store[];
lua_CFunction LuaFunctions::table_function_store[];
};  // namespace uActor::ActorRuntime
