// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

extern "C" {
void app_main(void);
}

#if BENCHMARK

#include "benchmark_actor.hpp"
#include "benchmark_lua_actor.hpp"
#include "router.hpp"

void actor_task(void* params) {
  RouterV2* router = ((struct Params*)params)->router;
  uint64_t id = ((struct Params*)params)->id;

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

  router->register_actor(id * ACTORS_PER_THREAD);
  for (int i = 1; i < ACTORS_PER_THREAD; i++) {
    router->register_alias(id * ACTORS_PER_THREAD, id * ACTORS_PER_THREAD + i);
  }

  while (true) {
    auto message = router->receive(id * ACTORS_PER_THREAD, portMAX_DELAY);
    if (message) {
      actor[message->receiver() - id * ACTORS_PER_THREAD]->receive(
          message->sender(), message->buffer());
    }
    // Without IO, taskYIELD() doesn't reset the idle task watchdog
    // vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void app_main(void) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());
  RouterV2* router = &RouterV2::getInstance();

  TaskHandle_t handles[NUM_THREADS] = {NULL};
  Params* params =
      static_cast<Params*>(pvPortMalloc(sizeof(Params) * NUM_THREADS));

  for (uint64_t i = 0; i < NUM_THREADS; i++) {
    params[i] = {.id = i, .router = router};
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "Test%lld", i);
    xTaskCreatePinnedToCore(&actor_task, buffer, 6168, &params[i], 5,
                            &handles[i], i % 2);
  }

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  router->send(0, 1, Message(STATIC_MESSAGE_SIZE));
}
#else

const char receive_fun[] = R"(function receive(sender, tag, message)
local ping_tag = 1025;
local pong_tag = 1026;
if(id == "actor/2" and tag == well_known_tags.init) then
  print(id.." received : "..message);
  for i = 3,33, 1
  do
    send("actor/"..i, ping_tag, "ping");
  end
  deferred_block_for("actor/33", "actor/2", pong_tag, 5000);
elseif(id == "actor/2") then
  print(id.." received : "..message);
elseif(id == "actor/3" and tag == ping_tag) then
  print(id.." received : "..message);
  deferred_block_for("actor/9999", "actor/9999", 9999, 2000);
  send(sender, 1027, "foo");
elseif(tag == ping_tag) then
  send(sender, pong_tag, "pong "..id);
  print(id.." received : "..message);
elseif(id == "actor/3" and tag == well_known_tags.timeout) then
  deferred_block_for("actor/9999", "actor/9999", 9999, 2000);
  print(id.." received expected timeout");
end
end)";

void app_main(void) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());
  TaskHandle_t handle = {NULL};
  Params params = {.id = "runtime/lua/1", .router = &RouterV2::getInstance()};
  xTaskCreatePinnedToCore(&LuaRuntime::os_task, "TEST", 6168, &params, 5,
                          &handle, 1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  char name[128];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    snprintf(name, sizeof(name), "actor/%d", 2 + i);
    char* buffer = new char[sizeof(receive_fun) + strlen(name) + 1];
    strncpy(buffer, name, strlen(name) + 1);
    // std::fill(buffer, buffer + 8 + sizeof(receive_fun), 0);
    std::memcpy((buffer + strlen(name) + 1), receive_fun, sizeof(receive_fun));
    Message m =
        Message("root", "runtime/lua/1", Tags::WELL_KNOWN_TAGS::SPAWN_LUA_ACTOR,
                buffer, sizeof(receive_fun) + strlen(name) + 1);
    RouterV2::getInstance().send(std::move(m));
    delete buffer;
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  RouterV2::getInstance().send(
      Message("root", "actor/2", Tags::WELL_KNOWN_TAGS::INIT, "start"));

  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
}

#endif
