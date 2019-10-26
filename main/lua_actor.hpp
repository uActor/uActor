#ifndef MAIN_LUA_ACTOR_HPP_
#define MAIN_LUA_ACTOR_HPP_

#include <utility>
#include "lua.hpp"

#include "benchmark_configuration.hpp"
#include "message.hpp"
#include "router.hpp"

Router* g_router;

#if TOUCH_DATA
#if TOUCH_BYTEWISE
const char receive_loop[] =
    R"(function receive(id, actors, message);
      local buffer = {};
      for i=1,#message do
        if((i-1)%75==0) then
          buffer[i]=2
        else
          buffer[i]=1
        end
      end
      send(id, (id+1)%actors, buffer);
      end)";
#else
const char receive_loop[] =
    R"(function receive(id, actors, message);
      local buffer = {};
      local step = 0;
      for i=1,#message/4 do
        if((i-1)*4==step) then
          buffer[i]=49<<24 | 49<<16 | 49<<8 | 50;
          step = step+75;
        elseif((i-1)*4+1==step) then
          buffer[i]=49<<24 | 49<<16 | 50<<8 | 49;
          step = step+75;
        elseif((i-1)*4+2==step) then
          buffer[i]=49<<24 | 50<<16 | 49<<8 | 49;
          step = step+75;
        elseif((i-1)*4+3==step) then
          buffer[i]=50<<24 | 49<<16 | 49<<8 | 49;
          step = step+75;
        else
          buffer[i]=49 | 49<<8 | 49<<16 | 49<<24 ;
        end
      end
      send(id, (id+1)%actors, buffer);
      end)";
#endif
#else
const char receive_loop[] =
    R"(function receive(id, actors, message);
      local buffer = {};
      send(id, (id+1)%actors, buffer);
      end)";
#endif

class LuaActor {
 public:
  LuaActor(uint64_t id, Router* router, lua_State* master_state)
      : id(id), router(router) {
    if (!id) g_router = router;
    round = 0;
    snprintf(actor_name, sizeof(actor_name), "actor_%lld", id);

    if (!master_state) {
      lua_state = create_state();
    } else {
      lua_state = master_state;
    }

    lua_newtable(lua_state);
    lua_getglobal(lua_state, "send");
    lua_setfield(lua_state, -2, "send");
    lua_getglobal(lua_state, "print");
    lua_setfield(lua_state, -2, "print");
    lua_setglobal(lua_state, actor_name);

    luaL_loadstring(lua_state, receive_loop);
    lua_getglobal(lua_state, actor_name);
    lua_setupvalue(lua_state, -2, 1);

    if (lua_pcall(lua_state, 0, 0, 0)) {
      printf("LUA LOAD ERROR!\n");
      printf("%s\n", lua_tostring(lua_state, 1));
      lua_pop(lua_state, 1);
    }
  }

  void receive(uint64_t sender, char* message) {
    // Measurement code is written in C to ensure precise measurements
    if (round == 0) {
      timestamp = xTaskGetTickCount();
    }
    if (round == 10 && id == 0) {
      printf("Measurement: %d\n",
             portTICK_PERIOD_MS * (xTaskGetTickCount() - timestamp));
      printf("IterationHeap: %d \n", xPortGetFreeHeapSize());
      fflush(stdout);
      round = 0;
      timestamp = xTaskGetTickCount();
    }
    if (id == 0) {
      round++;
    }

    lua_getglobal(lua_state, actor_name);
    lua_getfield(lua_state, -1, "receive");
    lua_replace(lua_state, 1);

    lua_pushinteger(lua_state, id);
    lua_pushinteger(lua_state, NUM_ACTORS);
    lua_pushlstring(lua_state, message, STATIC_MESSAGE_SIZE);

    if (lua_pcall(lua_state, 3, 0, 0)) {
      printf("LUA ERROR!\n");
      printf("%s\n", lua_tostring(lua_state, 1));
      lua_pop(lua_state, 1);
    }
    lua_gc(lua_state, LUA_GCCOLLECT, 0);
  }

  static lua_State* create_state() {
    lua_State* lua_state = luaL_newstate();

    luaL_requiref(lua_state, "_G", luaopen_base, 1);
    lua_pop(lua_state, 1);

    // luaL_requiref(lua_state, "math", luaopen_math, 1);
    // lua_pop(lua_state, 1);
    // luaL_requiref(lua_state, "coroutine", luaopen_coroutine, 1);
    // lua_pop(lua_state, 1);
    // luaL_requiref(lua_state, "string", luaopen_string, 1);
    // lua_pop(lua_state, 1);
    // luaL_requiref(lua_state, "table", luaopen_table, 1);
    // lua_pop(lua_state, 1);
    // luaL_requiref(lua_state, "utf8", luaopen_utf8, 1);
    // lua_pop(lua_state, 1);

    lua_pushcfunction(lua_state, send);
    lua_setglobal(lua_state, "send");
    return lua_state;
  }

 private:
  uint64_t id;
  char actor_name[32];
  Router* router;
  uint32_t timestamp;
  uint32_t round;
  lua_State* lua_state;

  static int send(lua_State* state) {
    uint64_t id = lua_tointeger(state, 1);
    uint64_t receiver = lua_tointeger(state, 2);
    Message m = Message();

    lua_pushnil(state);
    while (lua_next(state, 3) != 0) {
#if TOUCH_BYTEWISE
      if (lua_tointeger(state, -2) < STATIC_MESSAGE_SIZE + 1) {
        *(m.buffer() + lua_tointeger(state, -2) - 1) =
            48 + lua_tointeger(state, -1);
      }
#else
      if (lua_tointeger(state, -2) < STATIC_MESSAGE_SIZE / 4 + 1) {
        *reinterpret_cast<uint32_t*>(m.buffer() + lua_tointeger(state, -2) -
                                     1) = lua_tointeger(state, -1);
      }
#endif
      lua_pop(state, 1);
    }
    lua_pop(state, 1);
    // printf("[%d]%lld -> %lld: %.*s\n", xPortGetFreeHeapSize(), id, receiver,
    // STATIC_MESSAGE_SIZE, m.buffer());
    g_router->send(id, receiver, std::move(m));
    return 0;
  }
};
#endif  // MAIN_LUA_ACTOR_HPP_
