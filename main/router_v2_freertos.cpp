#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <mutex>
#include <optional>
#include <unordered_map>

#include "include/router_v2.hpp"

class RouterV2::Internal {
 public:
  void send(uint64_t sender, uint64_t receiver, Message message) {
    // std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(receiver);
    if (queue != routing_table.end()) {
      QueueHandle_t handle = queue->second;
      // lock.unlock();
      message.sender(sender);
      message.receiver(receiver);
      xQueueSend(handle, &message, portMAX_DELAY);
      message.moved();
    }
  }

  void register_actor(const uint64_t actor_id) {
    // TODO(raphaelhetzel) Potentially optimize by directly using freertos
    std::unique_lock<std::mutex> lock(mutex);
    if (routing_table.find(actor_id) == routing_table.end()) {
      routing_table.insert(
          std::make_pair(actor_id, xQueueCreate(QUEUE_SIZE, sizeof(Message))));
    } else {
      printf("tried to register already registered actor\n");
    }
  }

  void register_alias(const uint64_t actor_id, const uint64_t alias_id) {
    std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(actor_id);
    if (queue != routing_table.end()) {
      routing_table.insert(std::make_pair(alias_id, queue->second));
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

  std::optional<Message> receive(uint64_t receiver, size_t wait_time = 0) {
    std::unique_lock<std::mutex> lock(mutex);
    auto queue = routing_table.find(receiver);
    if (queue != routing_table.end()) {
      QueueHandle_t handle = queue->second;
      lock.unlock();  // This is not completely safe and assumes a queue is not
                      // read after delete is called
      auto message = Message(0, false);
      if (xQueueReceive(handle, &message, wait_time)) {
        // printf("%lld -> %lld\n", message.sender(), message.receiver());
        return message;
      }
    }
    return std::nullopt;
  }

 private:
  std::unordered_map<uint64_t, QueueHandle_t> routing_table;
  std::mutex mutex;
};

RouterV2::RouterV2() { _internal = new RouterV2::Internal(); }

RouterV2::~RouterV2() { delete _internal; }

void RouterV2::send(uint64_t sender, uint64_t receiver, Message&& message) {
  _internal->send(sender, receiver, std::move(message));
}
void RouterV2::register_actor(const uint64_t actor_id) {
  _internal->register_actor(actor_id);
}
void RouterV2::deregister_actor(uint64_t actor_id) {
  _internal->deregister_actor(actor_id);
}
void RouterV2::register_alias(const uint64_t actor_id,
                              const uint64_t alias_id) {
  _internal->register_alias(actor_id, alias_id);
}
std::optional<Message> RouterV2::receive(uint64_t receiver, size_t wait_time) {
  return _internal->receive(receiver, wait_time);
}
