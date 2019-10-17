// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>

extern "C" {
void app_main(void);
}

class Router {
 public:
  void register_actor(const uint64_t actor_id) {
    // TODO(raphaelhetzel) Potentially optimize by directly using freertos
    std::unique_lock<std::mutex> lock(mutex);
    printf("register\n");
    if (routing_table.find(actor_id) == routing_table.end()) {
      routing_table.insert(std::make_pair(
          actor_id, xQueueCreate(10, sizeof(uint32_t) + sizeof(uint64_t))));
    } else {
      printf("tried to register already registered actor\n");
    }
  }

  void deregister_actor(uint64_t actor_id) {
    std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(actor_id);
    if (queue != routing_table.end()) {
      vQueueDelete(queue->second);
      routing_table.erase(queue);
    }
  }

  void send(uint64_t sender, uint64_t receiver, int32_t message) {
    std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(receiver);
    if (queue != routing_table.end()) {
      QueueHandle_t handle = queue->second;
      lock.unlock();
      auto full_message = std::make_pair(sender, message);
      xQueueSend(handle, &full_message, 100 / portTICK_PERIOD_MS);
    }
  }

  bool receive(uint64_t receiver, uint64_t* sender, uint32_t* buffer) {
    std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(receiver);
    if (queue != routing_table.end()) {
      QueueHandle_t handle = queue->second;
      lock.unlock();  // This is not completely safe and assumes a queue is not
                      // read after delete is called
      std::pair<uint64_t, int32_t> full_message;
      if (xQueueReceive(handle, &full_message, portMAX_DELAY)) {
        *buffer = full_message.second;
        *sender = full_message.first;
        return true;
      }
    }
    return false;
  }

 private:
  std::unordered_map<uint64_t, QueueHandle_t> routing_table;
  std::mutex mutex;
};

class Actor {
 public:
  Actor(uint64_t id, Router* router) : id(id), router(router) {
    router->register_actor(id);
  }

  ~Actor() { router->deregister_actor(id); }

  int receive(uint64_t sender, uint32_t message) {
    printf("%lld -> %lld: %d\n", sender, id, message);
    send((id + 1) % 16, message);
    return 0;
  }

 private:
  uint64_t id;
  Router* router;

  int send(uint64_t receiver, uint32_t message) {
    router->send(id, receiver, message);
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
  uint32_t message;
  uint64_t sender;

  while (true) {
    if (router->receive(id, &sender, &message)) {
      actor->receive(sender, message);
    }
    // Without IO, taskYIELD() doesn't reset the idle task watchdog
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void app_main(void) {
  printf("%d \n", xPortGetFreeHeapSize());
  Router* router = new Router();

  TaskHandle_t handles[16] = {NULL};
  Params* params = static_cast<Params*>(pvPortMalloc(sizeof(Params) * 16));

  for (uint64_t i = 0; i < 16; i++) {
    params[i] = {.id = i, .router = router};
    vTaskDelay(100 / portTICK_PERIOD_MS);
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "Test%lld", i);
    xTaskCreate(&actor_task, buffer, 2048, &params[i], 5, &handles[i]);
  }

  vTaskDelay(1500 / portTICK_PERIOD_MS);
  printf("%d \n", xPortGetFreeHeapSize());
  router->send(0, 1, 16);
}
