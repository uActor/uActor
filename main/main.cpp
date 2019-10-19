// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

extern "C" {
void app_main(void);
}

class Message {
 public:
  Message(const Message& old) = delete;
  Message(Message&& old) : data_(old.data_) { old.data_ = nullptr; }

  Message() { data_ = new char[1508]; }

  ~Message() { delete data_; }

  uint64_t sender() { return *reinterpret_cast<uint64_t*>(data_); }

  void sender(uint64_t sendr) { *reinterpret_cast<uint64_t*>(data_) = sendr; }

  char* buffer() { return data_ + 8; }

  void* raw() { return data_; }

 private:
  char* data_;
};

class Router {
 public:
  void register_actor(const uint64_t actor_id) {
    // TODO(raphaelhetzel) Potentially optimize by directly using freertos
    std::unique_lock<std::mutex> lock(mutex);
    printf("register\n");
    if (routing_table.find(actor_id) == routing_table.end()) {
      routing_table.insert(std::make_pair(actor_id, xQueueCreate(1, 1508)));
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

  void send(uint64_t sender, uint64_t receiver, Message message) {
    std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(receiver);
    if (queue != routing_table.end()) {
      QueueHandle_t handle = queue->second;
      lock.unlock();
      message.sender(sender);
      xQueueSend(handle, message.raw(), 100 / portTICK_PERIOD_MS);
    }
  }

  std::optional<Message> receive(uint64_t receiver) {
    std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(receiver);
    if (queue != routing_table.end()) {
      QueueHandle_t handle = queue->second;
      lock.unlock();  // This is not completely safe and assumes a queue is not
                      // read after delete is called
      auto message = Message();
      if (xQueueReceive(handle, message.raw(), portMAX_DELAY)) {
        return message;
      }
    }
    return std::nullopt;
  }

 private:
  std::unordered_map<uint64_t, QueueHandle_t> routing_table;
  std::mutex mutex;
};

class Actor {
 public:
  Actor(uint64_t id, Router* router) : id(id), router(router) {
    router->register_actor(id);
    timestamp = xTaskGetTickCount();
    round = 0;
  }

  ~Actor() { router->deregister_actor(id); }

  void receive(uint64_t sender, char* message) {
    if (round == 10000 && id == 0) {
      printf("%lld: %d\n", id,
             portTICK_PERIOD_MS * (xTaskGetTickCount() - timestamp));
      fflush(stdout);
      round = 0;
      timestamp = xTaskGetTickCount();
    }
    if (id == 0) {
      round++;
    }
    Message m = Message();
    snprintf(m.buffer(), 1500, "Test %lld", id);
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
  router->send(0, 1, Message());
}
