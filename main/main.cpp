// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "benchmark_configuration.hpp"
#include "message.hpp"
#include "router.hpp"

extern "C" {
void app_main(void);
}

class Actor {
 public:
  Actor(uint64_t id, Router* router) : id(id), router(router) {
    router->register_actor(id);
    timestamp = xTaskGetTickCount();
    round = 0;
  }

  ~Actor() { router->deregister_actor(id); }

  void receive(uint64_t sender, char* message) {
    if (round == 0) {
      timestamp = xTaskGetTickCount();
    }
    if (round == 10000 && id == 0) {
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

    Message m = Message();
#if TOUCH_DATA
    for (int i = 0; i < STATIC_MESSAGE_SIZE;
         i += STATIC_MESSAGE_SIZE + (STATIC_MESSAGE_SIZE / 100)) {
      *(m.buffer() + i) = id % 255;
    }
#endif
    send((id + 1) % 16, std::move(m));
  }

 private:
  uint64_t id;
  Router* router;
  uint32_t timestamp;
  uint32_t round;

  int send(uint64_t receiver, Message&& message) {
    router->send(id, receiver, std::move(message));
    return 0;
  }
};

struct Params {
  uint64_t id;
  Router* router;
};

void actor_task(void* params) {
  Router* router = ((struct Params*)params)->router;
  uint64_t id = ((struct Params*)params)->id;

  Actor* actor = new Actor(id, router);

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
    xTaskCreatePinnedToCore(&actor_task, buffer, 2048, &params[i], 5,
                            &handles[i], i % 2);
  }

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  router->send(0, 1, Message(true));
}
