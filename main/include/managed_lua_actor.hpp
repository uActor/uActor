#ifndef MAIN_INCLUDE_MANAGED_LUA_ACTOR_HPP_
#define MAIN_INCLUDE_MANAGED_LUA_ACTOR_HPP_

#include <cstdio>
#include <list>
#include <string>
#include <utility>

#include "lua.hpp"
#include "managed_actor.hpp"
#include "message.hpp"

class ManagedLuaActor : public ManagedActor {
 public:
  ManagedLuaActor(RuntimeApi* api, uint32_t unique_id, const char* node_id,
                  const char* actor_type, const char* instance_id,
                  const char* code, lua_State* global_state)
      : ManagedActor(api, unique_id, node_id, actor_type, instance_id, code),
        state(global_state) {
    createActorEnvironment(code);
  }

  ~ManagedLuaActor() {
    lua_pushglobaltable(state);
    lua_pushnil(state);
    lua_seti(state, -2, id());
    lua_gc(state, LUA_GCCOLLECT, 0);
  }

  void receive(const Publication& m) {
    lua_pushglobaltable(state);
    lua_geti(state, -1, id());
    lua_getfield(state, -1, "receive");
    lua_replace(state, 1);
    lua_pop(state, 1);

    lua_newtable(state);
    for (const auto& value : m) {
      lua_pushstring(state, value.second.c_str());
      lua_setfield(state, -2, value.first.c_str());
    }

    if (lua_pcall(state, 1, 0, 0)) {
      printf("LUA ERROR!\n");
      printf("%s\n", lua_tostring(state, 1));
      lua_pop(state, 1);
    }

    lua_gc(state, LUA_GCCOLLECT, 0);
  }

 private:
  lua_State* state;

  static int send_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));
    Publication p = Publication(actor->node_id(), actor->actor_type(),
                                actor->instance_id());
    luaL_checktype(state, 1, LUA_TTABLE);
    lua_pushvalue(state, 1);
    lua_pushnil(state);

    while (lua_next(state, -2)) {
      lua_pushvalue(state, -2);
      std::string key(lua_tostring(state, -1));
      std::string value(lua_tostring(state, -2));
      p.set_attr(std::move(key), std::move(value));
      lua_pop(state, 2);
    }
    lua_pop(state, 1);
    actor->send(std::move(p));
    return 0;
  }

  static int deferred_sleep_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));
    uint32_t timeout = lua_tointeger(state, 1);
    actor->deferred_sleep(timeout);
    return 0;
  }

  static int deferred_block_for_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));

    uint32_t timeout = lua_tointeger(state, 2);
    actor->deffered_block_for(std::move(parse_filters(state, 1)), timeout);
    return 0;
  }

  static int subscribe_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));

    uint32_t id = actor->subscribe(parse_filters(state, 1));
    lua_pushinteger(state, id);
    return 1;
  }

  static int unsubscribe_wrapper(lua_State* state) {
    ManagedLuaActor* actor = reinterpret_cast<ManagedLuaActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));

    uint32_t sub_id = lua_tointeger(state, 1);
    actor->unsubscribe(sub_id);
    return 0;
  }

  static constexpr luaL_Reg core[] = {
      {"send", &send_wrapper},
      {"deferred_sleep", &deferred_sleep_wrapper},
      {"deferred_block_for", &deferred_block_for_wrapper},
      {"subscribe", &subscribe_wrapper},
      {"unsubscribe", &unsubscribe_wrapper},
      {NULL, NULL}};

  void createActorEnvironment(const char* receive_function) {
    lua_pushglobaltable(state);
    luaL_newlibtable(state, core);
    lua_pushlightuserdata(state, this);
    luaL_setfuncs(state, core, 1);

    lua_pushstring(state, node_id());
    lua_setfield(state, -2, "node_id");

    lua_pushstring(state, actor_type());
    lua_setfield(state, -2, "actor_type");

    lua_pushstring(state, instance_id());
    lua_setfield(state, -2, "instance_id");

    lua_getglobal(state, "print");
    lua_setfield(state, -2, "print");

    lua_getglobal(state, "tostring");
    lua_setfield(state, -2, "tostring");

    lua_getglobal(state, "tonumber");
    lua_setfield(state, -2, "tonumber");

    lua_createtable(state, 0, Tags::WELL_KNOWN_TAGS::FAKE_ITERATOR_END);
    for (uint32_t i = 1; i < Tags::WELL_KNOWN_TAGS::FAKE_ITERATOR_END; i++) {
      const char* name = Tags::tag_name(i);
      if (name) {
        lua_pushinteger(state, i);
        lua_setfield(state, -2, name);
      }
    }
    lua_setfield(state, -2, "well_known_tags");

    lua_seti(state, -2, id());

    luaL_loadstring(state, receive_function);

    // Sandbox the function by settings its
    // global environment to the table created above
    lua_geti(state, -2, id());
    lua_setupvalue(state, -2, 1);
    lua_replace(state, -2);

    if (lua_pcall(state, 0, 0, 0)) {
      printf("LUA LOAD ERROR!\n");
      printf("%s\n", lua_tostring(state, 1));
      lua_pop(state, 1);
    }
  }

  static PubSub::Filter parse_filters(lua_State* state, size_t index) {
    std::list<PubSub::Constraint> filter_list;
    luaL_checktype(state, index, LUA_TTABLE);
    lua_pushvalue(state, index);
    lua_pushnil(state);

    while (lua_next(state, -2)) {
      lua_pushvalue(state, -2);
      std::string key(lua_tostring(state, -1));
      std::string value(lua_tostring(state, -2));
      if (key.rfind("_optional_") == 0) {
        key = std::string("?") + key.substr(strlen("_optional_"));
      }
      filter_list.emplace_back(std::move(key), std::move(value));
      lua_pop(state, 2);
    }
    lua_pop(state, 1);

    return PubSub::Filter(std::move(filter_list));
  }
};

#endif  // MAIN_INCLUDE_MANAGED_LUA_ACTOR_HPP_
