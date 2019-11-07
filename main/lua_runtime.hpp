#ifndef MAIN_LUA_RUNTIME_HPP_
#define MAIN_LUA_RUNTIME_HPP_

#include <lua.hpp>
#include <utility>

#include "managed_actor.hpp"

const char receive_fun[] = R"(function receive(id);
      if(id == 2) then
        send(id+1);
        deferred_sleep(5000);
      elseif(id < 33) then
        send(id+1);
      end
      end)";

struct LuaAPI {
  static int send(lua_State* state) {
    ManagedActor* actor = reinterpret_cast<ManagedActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));
    uint64_t receiver = lua_tointeger(state, 1);
    Message m = Message();
    actor->send(receiver, std::move(m));
    return 0;
  }

  static int deferred_sleep(lua_State* state) {
    ManagedActor* actor = reinterpret_cast<ManagedActor*>(
        lua_touserdata(state, lua_upvalueindex(1)));
    uint32_t timeout = lua_tointeger(state, 1);
    actor->deferred_sleep(timeout);
    return 0;
  }
};

class ManagedLuaActor : public ManagedActor {
 public:
  ManagedLuaActor(uint64_t id, char* code, lua_State* global_state)
      : ManagedActor(id, code), state(global_state) {}

  void receive(const Message& m) {
    if (!init) {
      printf("%p\n", this);
      init = true;
      createActorEnvironment(receive_fun);
    }
    printf("receive %s\n", name());
    lua_getglobal(state, name());
    lua_getfield(state, -1, "receive");
    lua_replace(state, 1);

    lua_pushinteger(state, id());

    if (lua_pcall(state, 1, 0, 0)) {
      printf("LUA ERROR!\n");
      printf("%s\n", lua_tostring(state, 1));
      lua_pop(state, 1);
    }
  }

 private:
  bool init = false;
  lua_State* state;

  void createActorEnvironment(const char* receive_function) {
    printf("called create %s\n", name());
    lua_newtable(state);
    lua_pushlightuserdata(state, this);
    lua_pushcclosure(state, &LuaAPI::send, 1);
    lua_setfield(state, -2, "send");
    lua_pushlightuserdata(state, this);
    lua_pushcclosure(state, &LuaAPI::deferred_sleep, 1);
    lua_setfield(state, -2, "deferred_sleep");
    lua_setglobal(state, name());

    luaL_loadstring(state, receive_fun);

    lua_getglobal(state, name());
    lua_setupvalue(state, -2, 1);

    if (lua_pcall(state, 0, 0, 0)) {
      printf("LUA LOAD ERROR!\n");
      printf("%s\n", lua_tostring(state, 1));
      lua_pop(state, 1);
    }
  }
};

class LuaRuntime : public ActorRuntime<ManagedLuaActor> {
 public:
  // TODO(raphaelhetzel) make this generic again
  static void os_task(void* params) {
    Router* router = ((struct Params*)params)->router;
    uint64_t id = ((struct Params*)params)->id;

    LuaRuntime* art = new LuaRuntime(router, id);
    art->event_loop();
  }

  LuaRuntime(Router* router, uint64_t id)
      : ActorRuntime<ManagedLuaActor>(router, id) {
    state = create_lua_state();
  }

 private:
  lua_State* state;
  lua_State* create_lua_state() {
    lua_State* lua_state = luaL_newstate();
    luaL_requiref(lua_state, "_G", luaopen_base, 1);
    lua_pop(lua_state, 1);
    return lua_state;
  }

  ManagedLuaActor create_actor(uint64_t actor_id, char* code) {
    ManagedLuaActor a = ManagedLuaActor(actor_id, code, state);
    return std::move(a);
  }
};

#endif  // MAIN_LUA_RUNTIME_HPP_
