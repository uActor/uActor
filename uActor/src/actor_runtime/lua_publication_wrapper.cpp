#include "actor_runtime/lua_publication_wrapper.hpp"

namespace uActor::ActorRuntime {

int LuaPublicationWrapper::get(lua_State* state) {
  auto* pub_handle = static_cast<PubSub::Publication*>(
      luaL_checkudata(state, 1, "uActor.Publication"));
  return get_from_map(state, pub_handle->root_handle());
}

int LuaPublicationWrapper::gc(lua_State* state) {
  auto* publication = static_cast<PubSub::Publication*>(
      luaL_checkudata(state, 1, "uActor.Publication"));
  publication->~Publication();
  return 0;
}

int LuaPublicationWrapper::initialize(lua_State* state) {
  if (lua_gettop(state) % 2 > 0) {
    luaL_error(state, "bad publication");
  }

  PubSub::Publication p(lua_gettop(state) / 2);

  while (lua_gettop(state) != 0) {
    std::string_view key = std::string_view(luaL_checkstring(state, -2));

    if (lua_type(state, -1) == LUA_TSTRING) {
      std::string value(lua_tostring(state, -1));
      p.set_attr(key, std::move(value));
    } else if (lua_isinteger(state, -1) != 0) {
      int32_t value = lua_tointeger(state, -1);
      p.set_attr(key, value);
    } else if (lua_isnumber(state, -1) != 0) {
      float value = lua_tonumber(state, -1);
      p.set_attr(key, value);
    } else if (lua_istable(state, -1) != 0) {
      auto elem_handle = std::shared_ptr<PubSub::Publication::Map>();
      parse_table(elem_handle, state, -1);
      p.set_attr(key, elem_handle);
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
  auto* pub_handle = static_cast<PubSub::Publication*>(
      luaL_checkudata(state, 1, "uActor.Publication"));
  return set_in_map(state, pub_handle->root_handle());
}

int LuaPublicationWrapper::to_string(lua_State* state) {
  auto* publication = static_cast<PubSub::Publication*>(
      luaL_checkudata(state, 1, "uActor.Publication"));

  auto parsed = publication->root_handle()->to_string();
  lua_pushstring(state, parsed.c_str());
  return 1;
}

int LuaPublicationWrapper::luaopen(lua_State* state) {
  luaL_newmetatable(state, "uActor.Publication");
  const luaL_Reg meta_methods[] = {{"__index", &get},
                                   {"__gc", &gc},
                                   {"__newindex", &set},
                                   {"__tostring", &to_string},
                                   {nullptr, nullptr}};
  luaL_setfuncs(state, meta_methods, 0);
  lua_pop(state, 1);

  const luaL_Reg api[] = {{"new", &initialize}, {nullptr, nullptr}};
  luaL_newlib(state, api);
  return 1;
}

int LuaPublicationMapWrapper::get(lua_State* state) {
  auto* map_handle = static_cast<LuaPublicationMapWrapper*>(
      luaL_checkudata(state, 1, "uActor.PublicationMap"));
  return get_from_map(state, map_handle->data);
}

int LuaPublicationMapWrapper::gc(lua_State* state) {
  auto* publication_component = static_cast<LuaPublicationMapWrapper*>(
      luaL_checkudata(state, 1, "uActor.PublicationMap"));
  publication_component->~LuaPublicationMapWrapper();
  return 0;
}

int LuaPublicationMapWrapper::set(lua_State* state) {
  auto* map_handle = static_cast<LuaPublicationMapWrapper*>(
      luaL_checkudata(state, 1, "uActor.PublicationMap"));
  return set_in_map(state, map_handle->data);
}

int LuaPublicationMapWrapper::to_string(lua_State* state) {
  auto* publication_component = static_cast<PubSub::Publication::Map*>(
      luaL_checkudata(state, 1, "uActor.PublicationMap"));

  auto parsed = publication_component->to_string();
  lua_pushstring(state, parsed.c_str());
  return 1;
}

int LuaPublicationMapWrapper::luaopen(lua_State* state) {
  luaL_newmetatable(state, "uActor.PublicationMap");
  const luaL_Reg meta_methods[] = {{"__index", &get},
                                   {"__gc", &gc},
                                   {"__newindex", &set},
                                   {"__tostring", &to_string},
                                   {nullptr, nullptr}};
  luaL_setfuncs(state, meta_methods, 0);
  lua_pop(state, 1);

  const luaL_Reg api[] = {{nullptr, nullptr}};
  luaL_newlib(state, api);
  return 1;
}

int LuaPublicationBaseWrapper::get_from_map(
    lua_State* state, std::shared_ptr<PubSub::Publication::Map> map_handle) {
  auto key = std::string_view(luaL_checkstring(state, 2));
  if (!map_handle->has_attr(key)) {
    lua_pushnil(state);
    return 1;
  }
  auto value = map_handle->get_attr(key);
  if (std::holds_alternative<std::monostate>(value)) {
    lua_pushnil(state);
  } else if (std::holds_alternative<std::string_view>(value)) {
    lua_pushstring(state, std::get<std::string_view>(value).data());
  } else if (std::holds_alternative<int32_t>(value)) {
    lua_pushinteger(state, std::get<int32_t>(value));
  } else if (std::holds_alternative<float>(value)) {
    lua_pushnumber(state, std::get<float>(value));
  } else if (std::holds_alternative<std::shared_ptr<PubSub::Publication::Map>>(
                 value)) {
    LuaPublicationMapWrapper* lua_component =
        reinterpret_cast<LuaPublicationMapWrapper*>(
            lua_newuserdata(state, sizeof(LuaPublicationMapWrapper)));
    new (lua_component) LuaPublicationMapWrapper(
        std::get<std::shared_ptr<PubSub::Publication::Map>>(value));
    luaL_getmetatable(state, "uActor.PublicationMap");
    lua_setmetatable(state, -2);
  }
  return 1;
}

int LuaPublicationBaseWrapper::set_in_map(
    lua_State* state, std::shared_ptr<PubSub::Publication::Map> map_handle) {
  std::string_view key = std::string_view(luaL_checkstring(state, 2));

  if (lua_type(state, 3) == LUA_TSTRING) {
    lua_len(state, 3);
    size_t size = lua_tointeger(state, -1);
    lua_pop(state, 1);
    std::string value(lua_tolstring(state, 3, &size), size);
    map_handle->set_attr(key, std::move(value));
  } else if (lua_isinteger(state, 3) != 0) {
    int32_t value = lua_tointeger(state, 3);
    map_handle->set_attr(key, value);
  } else if (lua_isnumber(state, 3) != 0) {
    float value = lua_tonumber(state, 3);
    map_handle->set_attr(key, value);
  } else if (lua_istable(state, 3) != 0) {
    auto elem_handle = std::make_shared<PubSub::Publication::Map>();
    parse_table(elem_handle, state, 3);
    map_handle->set_attr(key, elem_handle);
  } else if (lua_isnil(state, 3)) {
    map_handle->erase_attr(key);
  }
  return 0;
}

void LuaPublicationBaseWrapper::parse_table(
    std::shared_ptr<PubSub::Publication::Map> component, lua_State* lua_state,
    int32_t index) {
  luaL_checktype(lua_state, index, LUA_TTABLE);
  lua_pushvalue(lua_state, index);
  lua_pushnil(lua_state);

  while (lua_next(lua_state, -2) != 0) {
    lua_pushvalue(lua_state, -2);
    std::string key(lua_tostring(lua_state, -1));

    if (lua_type(lua_state, -2) == LUA_TSTRING) {
      printf("called\n");
      std::string value(lua_tostring(lua_state, -2));
      component->set_attr(std::move(key), std::move(value));
    } else if (lua_isinteger(lua_state, -2) != 0) {
      int32_t value = lua_tointeger(lua_state, -2);
      component->set_attr(std::move(key), value);
    } else if (lua_isnumber(lua_state, -2) != 0) {
      float value = lua_tonumber(lua_state, -2);
      component->set_attr(std::move(key), value);
    } else if (lua_istable(lua_state, -2) != 0) {
      auto elem_handle = std::make_shared<PubSub::Publication::Map>();
      parse_table(elem_handle, lua_state, -2);
      component->set_attr(key, elem_handle);
    }
    lua_pop(lua_state, 2);
  }
  lua_pop(lua_state, 1);
}

}  // namespace uActor::ActorRuntime
