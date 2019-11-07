// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include <lua.hpp>

#include "actor.hpp"
#include "benchmark_configuration.hpp"
#include "lua_actor.hpp"
#include "lua_runtime.hpp"
#include "managed_actor.hpp"
#include "message.hpp"
#include "router.hpp"

extern "C" {
void app_main(void);
}

// struct Params {
//   uint64_t id;
//   Router* router;
// };

void actor_task(void* params) {
  Router* router = ((struct Params*)params)->router;
  uint64_t id = ((struct Params*)params)->id;

#if LUA_ACTORS
#if LUA_SHARED_RUNTIME
  lua_State* lua_state = LuaActor::create_state();
#else
  lua_State* lua_state = nullptr;
#endif
  LuaActor* actor[ACTORS_PER_THREAD];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    actor[i] = new LuaActor(id * ACTORS_PER_THREAD + i, router, lua_state);
  }
#else
  Actor* actor[ACTORS_PER_THREAD];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    actor[i] = new Actor(id * ACTORS_PER_THREAD + i, router);
  }
#endif

  router->register_actor(id * ACTORS_PER_THREAD);
  for (int i = 1; i < ACTORS_PER_THREAD; i++) {
    router->register_alias(id * ACTORS_PER_THREAD, id * ACTORS_PER_THREAD + i);
  }

  while (true) {
    auto message = router->receive(id * ACTORS_PER_THREAD);
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
  TaskHandle_t handle = {NULL};
  Params params = {.id = 1, .router = &Router::getInstance()};
  xTaskCreatePinnedToCore(&LuaRuntime::os_task, "TEST", 6168, &params, 5,
                          &handle, 1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    Message m = Message(true);
    *reinterpret_cast<uint64_t*>(m.buffer()) = 2 + i;
    Router::getInstance().send(0, 1, std::move(m));
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Router::getInstance().send(0, 2, Message(true));
  // ManagedActor a = ManagedActor(1, "");
  // ManagedActor b = ManagedActor();
  // Router* router = new Router();

  // TaskHandle_t handles[NUM_THREADS] = {NULL};
  // Params* params =
  //     static_cast<Params*>(pvPortMalloc(sizeof(Params) * NUM_THREADS));

  // for (uint64_t i = 0; i < NUM_THREADS; i++) {
  //   params[i] = {.id = i, .router = router};
  //   char buffer[16];
  //   snprintf(buffer, sizeof(buffer), "Test%lld", i);
  //   xTaskCreatePinnedToCore(&actor_task, buffer, 6168, &params[i], 5,
  //                           &handles[i], i % 2);
  // }

  // vTaskDelay(1000 / portTICK_PERIOD_MS);

  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  // router->send(0, 1, Message(true));
}
