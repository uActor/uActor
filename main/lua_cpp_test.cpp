
// Simple test to verify that both LUA and modern C++ are working

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <lua.hpp>

extern "C" {
void app_main(void);
}

class Test {
 public:
  uint32_t heapSpace() { return xPortGetFreeHeapSize(); }
};

void test_task(void *) {
  std::unique_ptr<Test> test = std::make_unique<Test>();

  printf("Task before LUA: %d \n", xPortGetFreeHeapSize());
  lua_State *lua = luaL_newstate();
  printf("Task after LUA: %d \n", xPortGetFreeHeapSize());

  luaL_requiref(lua, "_G", luaopen_base, 1);
  lua_pop(lua, 1);
  luaL_requiref(lua, "math", luaopen_math, 1);
  lua_pop(lua, 1);
  luaL_requiref(lua, "coroutine", luaopen_coroutine, 1);
  lua_pop(lua, 1);
  luaL_requiref(lua, "string", luaopen_string, 1);
  lua_pop(lua, 1);
  luaL_requiref(lua, "utf8", luaopen_utf8, 1);
  lua_pop(lua, 1);

  printf("Task after base load: %d \n", xPortGetFreeHeapSize());

  if (luaL_dostring(lua, "function test(); print('test');end")) {
    printf("LUA LOAD ERROR!\n");
    printf("%s\n", lua_tostring(lua, 1));
    lua_pop(lua, 1);
  }

  while (1) {
    luaL_loadstring(lua, "test()");
    if (lua_pcall(lua, 0, 0, 0)) {
      printf("LUA ERROR!\n");
      printf("%s\n", lua_tostring(lua, 1));
      lua_pop(lua, 1);
    }
    lua_gc(lua, LUA_GCCOLLECT, 0);
    printf("Available Heap Space: %d\n", test->heapSpace());
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void app_main(void) {
  TaskHandle_t handle = NULL;
  xTaskCreate(&test_task, "Test", 4096, 0, 5, &handle);
}
