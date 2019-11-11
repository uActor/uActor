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
    printf("receive %s: size: %d\n", name(), m.size());
    lua_getglobal(state, name());
    lua_getfield(state, -1, "receive");
    lua_replace(state, 1);

    lua_pushinteger(state, id());

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
    uint64_t receiver = lua_tointeger(state, 1);
    Message m = Message(0);
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

  static constexpr luaL_Reg core[] = {
      {"send", &send_wrapper},
      {"deferred_sleep", &deferred_sleep_wrapper},
      {NULL, NULL}};

  void createActorEnvironment(const char* receive_function) {
    printf("called create %s\n", name());
    luaL_newlibtable(state, core);
    lua_pushlightuserdata(state, this);
    luaL_setfuncs(state, core, 1);
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
