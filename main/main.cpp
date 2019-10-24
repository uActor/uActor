// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "benchmark_configuration.hpp"
#if LUA_ACTORS
#include "lua_actor.hpp"
#else
#include "actor.hpp"
#endif
#include "message.hpp"
#include "router.hpp"

extern "C" {
void app_main(void);
}

struct Params {
  uint64_t id;
  Router* router;
};

void actor_task(void* params) {
  Router* router = ((struct Params*)params)->router;
  uint64_t id = ((struct Params*)params)->id;

#if LUA_ACTORS
  LuaActor* actor = new LuaActor(id, router);
#else
  Actor* actor = new Actor(id, router);
#endif

  while (true) {
    auto message = router->receive(id);
    if (message) {
      actor->receive((*message).sender(), (*message).buffer());
    }
    // Without IO, taskYIELD() doesn't reset the idle task watchdog
    // vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void app_main(void) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());
  Router* router = new Router();

  TaskHandle_t handles[16] = {NULL};
  Params* params = static_cast<Params*>(pvPortMalloc(sizeof(Params) * 16));

  for (uint64_t i = 0; i < 16; i++) {
    params[i] = {.id = i, .router = router};
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "Test%lld", i);
    xTaskCreatePinnedToCore(&actor_task, buffer, 6168, &params[i], 5,
                            &handles[i], i % 2);
  }

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  router->send(0, 1, Message(true));
}
