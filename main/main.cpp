// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

#define STATIC_MESSAGE_SIZE 1500

// Write out the id to the task in 8 byte
// steps to touch the data in some way
#define TOUCH_DATA true

// TRUE: Message copied to the queue FALSE: Reference copied to the queue
#define BY_VALUE false

#define QUEUE_SIZE 1

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

  explicit Message(bool init = true) {
    if (init) {
      data_ = new char[STATIC_MESSAGE_SIZE + 8];
    }
  }

  ~Message() { delete data_; }

  void moved() { data_ = nullptr; }

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
    if (routing_table.find(actor_id) == routing_table.end()) {
      routing_table.insert(
#if BY_VALUE
          std::make_pair(actor_id,
                         xQueueCreate(QUEUE_SIZE, STATIC_MESSAGE_SIZE + 8)));
#else
          std::make_pair(actor_id, xQueueCreate(QUEUE_SIZE, sizeof(Message))));
#endif
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
#if BY_VALUE
      xQueueSend(handle, message.raw(), portMAX_DELAY);
#else
      xQueueSend(handle, &message, portMAX_DELAY);
      message.moved();
#endif
    }
  }

  std::optional<Message> receive(uint64_t receiver) {
    std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(receiver);
    if (queue != routing_table.end()) {
      QueueHandle_t handle = queue->second;
      lock.unlock();  // This is not completely safe and assumes a queue is not
                      // read after delete is called
#if BY_VALUE
      auto message = Message(true);
      if (xQueueReceive(handle, message.raw(), portMAX_DELAY)) {
        return message;
      }
#else
      auto message = Message(false);
      if (xQueueReceive(handle, &message, portMAX_DELAY)) {
        return message;
      }
#endif
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
    if (round == 0) {
      timestamp = xTaskGetTickCount();
    }
    if (round == 10000 && id == 0) {
      printf("Measurement: %d\n",
             portTICK_PERIOD_MS * (xTaskGetTickCount() - timestamp));
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
