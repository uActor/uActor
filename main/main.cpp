// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
extern "C" {
#include <esp_system.h>
}

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include <lua.hpp>

#include "benchmark_configuration.hpp"
#include "include/board_functions.hpp"
#include "include/lua_runtime.hpp"
#include "include/managed_actor.hpp"
#include "include/message.hpp"
#include "include/router_v2.hpp"
#include "tcp_forwarder.hpp"
#include "wifi_stack.hpp"

extern "C" {
void app_main(void);
}

#if BENCHMARK

#include "benchmark_actor.hpp"
#include "benchmark_lua_actor.hpp"
// #include "router.hpp"

struct BenchmarkParams {
  uint64_t id;
  RouterV2* router;
};

void actor_task(void* params) {
  RouterV2* router = ((struct BenchmarkParams*)params)->router;
  uint64_t id = ((struct BenchmarkParams*)params)->id;

#if LUA_ACTORS
#if LUA_SHARED_RUNTIME
  lua_State* lua_state = LuaActor::create_state();
#else
  lua_State* lua_state = nullptr;
#endif
  LuaActor* actor[ACTORS_PER_THREAD];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    actor[i] = new LuaActor(id * ACTORS_PER_THREAD + i, lua_state);
  }
#else
  Actor* actor[ACTORS_PER_THREAD];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    actor[i] = new Actor(id * ACTORS_PER_THREAD + i);
  }
#endif
  char primary[128], buffer[128];
  snprintf(primary, sizeof(primary), "%lld", id * ACTORS_PER_THREAD);
  router->register_actor(primary);
  for (int i = 1; i < ACTORS_PER_THREAD; i++) {
    snprintf(buffer, sizeof(buffer), "%lld", id * ACTORS_PER_THREAD + i);
    router->register_alias(primary, buffer);
  }

  while (true) {
    auto message = router->receive(primary, portMAX_DELAY);
    if (message) {
      actor[std::stoi(message->receiver()) - id * ACTORS_PER_THREAD]->receive(
          std::stoi(message->sender()), const_cast<char*>(message->buffer()));
    }
  }
}

void real_main(void*) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());
  RouterV2* router = &RouterV2::getInstance();

  // TaskHandle_t handles[NUM_THREADS] = {NULL};
  BenchmarkParams* params = static_cast<BenchmarkParams*>(
      pvPortMalloc(sizeof(BenchmarkParams) * NUM_THREADS));

  for (uint64_t i = 0; i < NUM_THREADS; i++) {
    params[i] = {.id = i, .router = router};
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "Test%lld", i);
    xTaskCreatePinnedToCore(&actor_task, buffer, 7500, &params[i], 5, nullptr,
                            i % 2);
  }

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  router->send(Message("0", "1", 1234, ""));
  vTaskDelete(nullptr);
}

void app_main(void) {
  xTaskCreatePinnedToCore(&real_main, "MAIN", 7500, nullptr, 5, nullptr, 0);
}
#else

const char receive_fun[] = R"(function receive(sender, tag, message)
local ping_tag = 1025;
local pong_tag = 1026;
if(id == "actor/local/2" and tag == well_known_tags.init) then
  print(id.." received : "..message);
  send("actor/local/#", ping_tag, "ping");
  deferred_block_for("actor/local/33", "actor/local/2", pong_tag, 5000);
elseif(id == "actor/local/2") then
  print(id.." received : "..message);
elseif(id == "actor/local/3" and tag == ping_tag) then
  print(id.." received : "..message);
  deferred_block_for("actor/local/9999", "actor/local/9999", 9999, 2000);
  send(sender, 1027, "foo");
elseif(tag == ping_tag) then
  send(sender, pong_tag, "pong "..id);
  print(id.." received : "..message);
elseif(id == "actor/local/3" and tag == well_known_tags.timeout) then
  deferred_block_for("actor/local/9999", "actor/local/9999", 9999, 2000);
  send("actor/local/45", ping_tag, "ping");
  print(id.." received expected timeout");
end
end)";

void main_task(void*) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());
  TaskHandle_t handle = {NULL};
  Params params = {.id = "core.runtime.lua/local/1",
                   .router = &RouterV2::getInstance()};
  xTaskCreatePinnedToCore(&LuaRuntime::os_task, "TEST", 6168, &params, 7,
                          &handle, 1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  char name[128];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    snprintf(name, sizeof(name), "actor/local/%d", 2 + i);
    char* buffer = new char[sizeof(receive_fun) + strlen(name) + 1];
    strncpy(buffer, name, strlen(name) + 1);
    // std::fill(buffer, buffer + 8 + sizeof(receive_fun), 0);
    std::memcpy((buffer + strlen(name) + 1), receive_fun, sizeof(receive_fun));
    Message m = Message("root", "core.runtime.lua/local/1",
                        Tags::WELL_KNOWN_TAGS::SPAWN_LUA_ACTOR, buffer,
                        sizeof(receive_fun) + strlen(name) + 1);
    RouterV2::getInstance().send(std::move(m));
    delete buffer;
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  RouterV2::getInstance().send(
      Message("root", "actor/local/2", Tags::WELL_KNOWN_TAGS::INIT, "start"));

  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  while (true) {
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Heap: %d \n", xPortGetFreeHeapSize());
  }
}

void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  xTaskCreatePinnedToCore(&main_task, "MAIN", 6168, nullptr, 5, nullptr, 1);
  /* Network forwarding experiments
  xTaskCreatePinnedToCore(&WifiStack::os_task, "FORWARDER", 6168, nullptr, 4,
                          nullptr, 1);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  xTaskCreatePinnedToCore(&TCPForwarder::os_task, "TCP", 6168, nullptr, 4,
                          nullptr, 1);
  */
}

#endif
