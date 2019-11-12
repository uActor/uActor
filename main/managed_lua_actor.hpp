#ifndef MAIN_MANAGED_LUA_ACTOR_HPP_
#define MAIN_MANAGED_LUA_ACTOR_HPP_

#include <cstdio>
#include <utility>

#include <lua.hpp>

#include "managed_actor.hpp"
#include "message.hpp"

class ManagedLuaActor : public ManagedActor {
 public:
  ManagedLuaActor(uint64_t id, char* cin, lua_State* global_state)
      : ManagedActor(id, cin), state(global_state) {
    createActorEnvironment(code());
  }

  void receive(const Message& m) {
    // printf("receive %s: size: %d\n", name(), m.size());
    lua_getglobal(state, name());
    lua_getfield(state, -1, "receive");
    lua_replace(state, 1);

    lua_pushinteger(state, m.sender());
    lua_pushinteger(state, m.tag());
    if (m.size() > 0) {
      lua_pushstring(state, m.ro_buffer());
    } else {
      lua_pushnil(state);
    }

    if (lua_pcall(state, 3, 0, 0)) {
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
    uint64_t receiver = lua_tointeger(state, 1);
    Message m = Message(0);
    size_t message_size = 0;
    if (lua_isstring(state, 2)) {
      const char* message = lua_tostring(state, 3);
      message_size = strlen(message) + 1;
      m = Message(message_size);
      std::strncpy(m.buffer(), message, strlen(message) + 1);
    }

    m.tag(lua_tointeger(state, 2));

    actor->send(receiver, std::move(m));
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
    uint32_t sender = lua_tointeger(state, 1);
    uint32_t receiver = lua_tointeger(state, 2);
    uint32_t tag = lua_tointeger(state, 3);
    uint32_t timeout = lua_tointeger(state, 4);
    actor->deffered_block_for(sender, receiver, tag, timeout);
    return 0;
  }

  static constexpr luaL_Reg core[] = {
      {"send", &send_wrapper},
      {"deferred_sleep", &deferred_sleep_wrapper},
      {"deferred_block_for", &deferred_block_for_wrapper},
      {NULL, NULL}};

  void createActorEnvironment(const char* receive_function) {
    printf("called create %s\n", name());
    luaL_newlibtable(state, core);
    lua_pushlightuserdata(state, this);
    luaL_setfuncs(state, core, 1);

    lua_pushinteger(state, id());
    lua_setfield(state, -2, "id");

    lua_getglobal(state, "print");
    lua_setfield(state, -2, "print");

    lua_createtable(state, 0, Tags::WELL_KNOWN_TAGS::FAKE_ITERATOR_END);
    for (uint32_t i = 1; i < Tags::WELL_KNOWN_TAGS::FAKE_ITERATOR_END; i++) {
      const char* name = Tags::tag_name(i);
      if (name) {
        lua_pushinteger(state, i);
        lua_setfield(state, -2, name);
      }
    }
    lua_setfield(state, -2, "well_known_tags");

    // lua_newtable(state);
    // lua_pushlightuserdata(state, this);
    // lua_pushcclosure(state, &send_wrapper, 1);
    // lua_setfield(state, -2, "send");
    // lua_pushlightuserdata(state, this);
    // lua_pushcclosure(state, &deferred_sleep_wrapper, 1);
    // lua_setfield(state, -2, "deferred_sleep");
    lua_setglobal(state, name());

    luaL_loadstring(state, receive_function);

    // Sandboxing the actor
    lua_getglobal(state, name());
    lua_setupvalue(state, -2, 1);

    if (lua_pcall(state, 0, 0, 0)) {
      printf("LUA LOAD ERROR!\n");
      printf("%s\n", lua_tostring(state, 1));
      lua_pop(state, 1);
    }
  }
};

#endif  // MAIN_MANAGED_LUA_ACTOR_HPP_
