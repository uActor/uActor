#import "actor_runtime/lua_publication_wrapper.hpp"

namespace uActor::ActorRuntime {

int LuaPublicationWrapper::get(lua_State* state) {
  auto publication =
      (PubSub::Publication*)luaL_checkudata(state, 1, "uActor.Publication");
  auto key = std::string_view(luaL_checkstring(state, 2));
  auto value = publication->get_attr(key);
  if (std::holds_alternative<std::monostate>(value)) {
    lua_pushnil(state);
  } else if (std::holds_alternative<std::string_view>(value)) {
    lua_pushstring(state, std::get<std::string_view>(value).data());
  } else if (std::holds_alternative<int32_t>(value)) {
    lua_pushinteger(state, std::get<int32_t>(value));
  } else if (std::holds_alternative<float>(value)) {
    lua_pushnumber(state, std::get<float>(value));
  }
  return 1;
}

int LuaPublicationWrapper::gc(lua_State* state) {
  auto publication =
      (PubSub::Publication*)luaL_checkudata(state, 1, "uActor.Publication");
  publication->~Publication();
  return 0;
}

int LuaPublicationWrapper::initialize(lua_State* state) {
  if (lua_gettop(state) % 2 > 0) {
    luaL_error(state, "bad publication");
  }

  PubSub::Publication p(lua_gettop(state) / 2);

  while (lua_gettop(state)) {
    std::string_view key = std::string_view(luaL_checkstring(state, -2));

    if (lua_type(state, -1) == LUA_TSTRING) {
      std::string value(lua_tostring(state, -1));
      p.set_attr(key, std::move(value));
    } else if (lua_isinteger(state, -1)) {
      int32_t value = lua_tointeger(state, -1);
      p.set_attr(key, value);
    } else if (lua_isnumber(state, -1)) {
      float value = lua_tonumber(state, -1);
      p.set_attr(key, value);
    }

    lua_pop(state, 2);
  }

  PubSub::Publication* lua_pub = reinterpret_cast<PubSub::Publication*>(
      lua_newuserdata(state, sizeof(PubSub::Publication)));
  new (lua_pub) PubSub::Publication(std::move(p));
  luaL_getmetatable(state, "uActor.Publication");
  lua_setmetatable(state, -2);
  return 1;
}

int LuaPublicationWrapper::set(lua_State* state) {
  auto p =
      (PubSub::Publication*)luaL_checkudata(state, 1, "uActor.Publication");
  std::string_view key = std::string_view(luaL_checkstring(state, 2));

  if (lua_type(state, 3) == LUA_TSTRING) {
    std::string value(lua_tostring(state, 3));
    p->set_attr(key, std::move(value));
  } else if (lua_isinteger(state, 3)) {
    int32_t value = lua_tointeger(state, 3);
    p->set_attr(key, value);
  } else if (lua_isnumber(state, 3)) {
    float value = lua_tonumber(state, 3);
    p->set_attr(key, value);
  }
  return 0;
}

int LuaPublicationWrapper::luaopen(lua_State* state) {
  luaL_newmetatable(state, "uActor.Publication");
  const luaL_Reg meta_methods[] = {
      {"__index", &get}, {"__gc", &gc}, {"__newindex", &set}, {NULL, NULL}};
  luaL_setfuncs(state, meta_methods, 0);
  lua_pop(state, 1);

  const luaL_Reg api[] = {{"new", &initialize}, {NULL, NULL}};
  luaL_newlib(state, api);
  return 1;
}

}  // namespace uActor::ActorRuntime
