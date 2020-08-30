#include "actor_runtime/lua_executor.hpp"

#include <utility>

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

lua_State* LuaExecutor::create_lua_state() {
  lua_State* lua_state = luaL_newstate();
  luaL_requiref(lua_state, "_G", luaopen_base, 1);
  lua_pop(lua_state, 1);
  luaL_requiref(lua_state, "math", luaopen_math, 1);
  lua_pop(lua_state, 1);
  luaL_requiref(lua_state, "string", luaopen_string, 1);
  lua_pop(lua_state, 1);
  luaL_requiref(lua_state, "Publication", LuaPublicationWrapper::luaopen, 1);
  lua_pop(lua_state, 1);
  lua_gc(lua_state, LUA_GCSETSTEPMUL, 200);
  lua_gc(lua_state, LUA_GCSETPAUSE, 50);
  return lua_state;
}
}  // namespace uActor::ActorRuntime
