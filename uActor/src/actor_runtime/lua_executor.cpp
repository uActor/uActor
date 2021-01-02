#include "actor_runtime/lua_executor.hpp"

#include <cmath>
#include <string_view>
#include <utility>

#include "actor_runtime/lua_functions.hpp"
#include "actor_runtime/lua_publication_wrapper.hpp"

namespace uActor::ActorRuntime {

LuaExecutor::LuaExecutor(PubSub::Router* router, const char* node_id,
                         const char* id)
    : Executor<ManagedLuaActor, LuaExecutor>(router, node_id, "lua_executor",
                                             id) {
  state = create_lua_state();

  PubSub::Publication p{BoardFunctions::NODE_ID, "lua_executor", "1"};
  p.set_attr("type", "executor_update");
  p.set_attr("command", "register");
  p.set_attr("actor_runtime_type", "lua");
  p.set_attr("update_node_id", BoardFunctions::NODE_ID);
  p.set_attr("update_actor_type", "lua_executor");
  p.set_attr("update_instance_id", "1");
  p.set_attr("node_id", BoardFunctions::NODE_ID);
  PubSub::Router::get_instance().publish(std::move(p));
}

LuaExecutor::~LuaExecutor() {
  PubSub::Publication p{BoardFunctions::NODE_ID, "lua_executor", "1"};
  p.set_attr("type", "executor_update");
  p.set_attr("command", "deregister");
  p.set_attr("actor_runtime_type", "lua");
  p.set_attr("update_node_id", BoardFunctions::NODE_ID);
  p.set_attr("update_actor_type", "lua_executor");
  p.set_attr("update_instance_id", "1");
  p.set_attr("node_id", BoardFunctions::NODE_ID);
  uActor::PubSub::Router::get_instance().publish(std::move(p));

  actors.clear();
  lua_close(state);
}

void LuaExecutor::open_lua_base_optimized(lua_State* lua_state) {
  // The Lua base methods are declared static.
  // This is our current way of accessing them without patching the Lua
  // codebase.
  lua_State* tmp_state = luaL_newstate();
  luaL_requiref(tmp_state, "_G", luaopen_base, 1);
  lua_pop(tmp_state, 1);

  lua_pushglobaltable(tmp_state);
  for (auto [key, value] : LuaFunctions::base_functions) {
    lua_getfield(tmp_state, -1, key.data());
    lua_CFunction fun = lua_tocfunction(tmp_state, -1);
    if (fun != nullptr) {
      *(value.second) = fun;
    }
    lua_pop(tmp_state, 1);
  }

  lua_close(tmp_state);

  lua_pushglobaltable(lua_state);
  lua_newtable(lua_state);
  lua_pushcfunction(lua_state, base_index);
  lua_setfield(lua_state, -2, "__index");
  lua_setmetatable(lua_state, -2);

  lua_pushvalue(lua_state, -1);
  lua_setfield(lua_state, -2, "_G");
  lua_pushliteral(lua_state, LUA_VERSION);
  lua_setfield(lua_state, -2, "_VERSION");
  lua_pop(lua_state, 1);
}

void LuaExecutor::open_lua_math_optimized(lua_State* lua_state) {
  // The Lua base methods are declared static.
  // This is our current way of accessing them without patching the Lua
  // codebase.
  lua_State* tmp_state = luaL_newstate();
  luaL_requiref(tmp_state, "_G", luaopen_base, 1);
  lua_pop(tmp_state, 1);
  luaL_requiref(tmp_state, "math", luaopen_math, 1);
  lua_pop(tmp_state, 1);

  lua_getglobal(tmp_state, "math");
  for (auto [key, value] : math_functions) {
    lua_getfield(tmp_state, -1, key.data());
    lua_CFunction fun = lua_tocfunction(tmp_state, -1);
    if (fun != nullptr) {
      *value = fun;
    }
    lua_pop(tmp_state, 1);
  }

  lua_close(tmp_state);

  lua_newtable(lua_state);
  lua_newtable(lua_state);
  lua_pushcfunction(lua_state, math_index);
  lua_setfield(lua_state, -2, "__index");
  lua_setmetatable(lua_state, -2);

  lua_pushnumber(lua_state, (l_mathop(3.141592653589793238462643383279502884)));
  lua_setfield(lua_state, -2, "pi");
  lua_pushnumber(lua_state, static_cast<lua_Number>(INFINITY));
  lua_setfield(lua_state, -2, "huge");
  lua_pushinteger(lua_state, LUA_MAXINTEGER);
  lua_setfield(lua_state, -2, "maxinteger");
  lua_pushinteger(lua_state, LUA_MININTEGER);
  lua_setfield(lua_state, -2, "mininteger");

  lua_setglobal(lua_state, "math");
}

void LuaExecutor::open_lua_string_optimized(lua_State* lua_state) {
  // The Lua base methods are declared static.
  // This is our current way of accessing them without patching the Lua
  // codebase.
  lua_State* tmp_state = luaL_newstate();
  luaL_requiref(tmp_state, "_G", luaopen_base, 1);
  lua_pop(tmp_state, 1);
  luaL_requiref(tmp_state, "string", luaopen_string, 1);
  lua_pop(tmp_state, 1);

  lua_getglobal(tmp_state, "string");
  for (auto [key, value] : string_functions) {
    lua_getfield(tmp_state, -1, key.data());
    lua_CFunction fun = lua_tocfunction(tmp_state, -1);
    if (fun != nullptr) {
      *value = fun;
    } else {
      printf("%d\n", lua_type(tmp_state, -1));
    }
    lua_pop(tmp_state, 1);
  }

  lua_close(tmp_state);

  lua_newtable(lua_state);
  lua_pushliteral(lua_state, "");

  lua_newtable(lua_state);
  lua_pushcfunction(lua_state, string_index);
  lua_setfield(lua_state, -2, "__index");
  lua_pushvalue(lua_state, -1);

  lua_setmetatable(lua_state, -3);
  lua_setmetatable(lua_state, -3);
  lua_pop(lua_state, 1);

  lua_setglobal(lua_state, "string");
}

int LuaExecutor::base_index(lua_State* state) {
  const auto* key = lua_tostring(state, 2);

  const auto* function = LuaFunctions::base_functions.find(frozen::string(key));
  if (function != LuaFunctions::base_functions.end()) {
    lua_pushcfunction(state, *(function->second.second));
  } else {
    lua_pushnil(state);
  }
  return 1;
}

int LuaExecutor::math_index(lua_State* state) {
  const auto* key = lua_tostring(state, 2);

  const auto* function = math_functions.find(frozen::string(key));
  if (function != math_functions.end()) {
    lua_pushcfunction(state, *(function->second));
  } else {
    lua_pushnil(state);
  }
  return 1;
}

int LuaExecutor::string_index(lua_State* state) {
  const auto* key = lua_tostring(state, 2);

  const auto* function = string_functions.find(frozen::string(key));
  if (function != string_functions.end()) {
    lua_pushcfunction(state, *(function->second));
  } else {
    lua_pushnil(state);
  }
  return 1;
}

lua_CFunction LuaExecutor::math_function_store[];
lua_CFunction LuaExecutor::string_function_store[];

lua_State* LuaExecutor::create_lua_state() {
  lua_State* lua_state =
      lua_newstate(uActor::Support::MemoryManager::allocate_lua, nullptr);
  open_lua_base_optimized(lua_state);
  open_lua_math_optimized(lua_state);
  open_lua_string_optimized(lua_state);
  luaL_requiref(lua_state, "Publication", LuaPublicationWrapper::luaopen, 1);
  lua_pop(lua_state, 1);
  lua_gc(lua_state, LUA_GCSETSTEPMUL, 200);
  lua_gc(lua_state, LUA_GCSETPAUSE, 50);
  return lua_state;
}
}  // namespace uActor::ActorRuntime
