#include "router.hpp"
#include "benchmark_configuration.hpp"

#if COND_VAR
void Router::register_actor(const uint64_t actor_id) {
  // TODO(raphaelhetzel) Potentially optimize by directly using freertos
  std::unique_lock<std::mutex> lock(mutex);
  if (routing_table.find(actor_id) == routing_table.end()) {
    routing_table.insert(std::make_pair(actor_id, TableEntry()));
  } else {
    printf("tried to register already registered actor\n");
  }
}
void Router::register_alias(const uint64_t actor_id, const uint64_t alias_id) {
  // Currently not implemented
  assert(false);
}

void Router::deregister_actor(uint64_t actor_id) {
  std::unique_lock<std::mutex> lock(mutex);
  auto queue = routing_table.find(actor_id);
  if (queue != routing_table.end()) {
    routing_table.erase(queue);
  }
}

void Router::send(uint64_t sender, uint64_t receiver, Message message) {
  std::unique_lock<std::mutex> lock(mutex);
  auto queue = routing_table.find(receiver);
  if (queue != routing_table.end()) {
    TableEntry& handle = queue->second;
    lock.unlock();
    std::unique_lock<std::mutex> inner_lock(*handle.mutex);
    message.sender(sender);
    message.receiver(receiver);
#if BY_VALUE
#else
    handle.queue = std::move(message);
    handle.items++;
    inner_lock.unlock();
    handle.cv->notify_one();
#endif
  }
}

std::optional<Message> Router::receive(uint64_t receiver) {
  std::unique_lock<std::mutex> lock(mutex);
  auto queue = routing_table.find(receiver);
  if (queue != routing_table.end()) {
    TableEntry& handle = queue->second;
    lock.unlock();  // This is not completely safe and assumes a queue is not
                    // read after delete is called
    std::unique_lock<std::mutex> inner_lock(*handle.mutex);
    handle.cv->wait(inner_lock, [&]() { return handle.items > 0; });
#if BY_VALUE
#else
    handle.items--;
    return std::move(handle.queue);
    // handle.queue.pop();
    // return std::move(m);
#endif
  }
  return std::nullopt;
}

#else
void Router::register_actor(const uint64_t actor_id) {
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

void Router::register_alias(const uint64_t actor_id, const uint64_t alias_id) {
  std::unique_lock<std::mutex> lock(mutex);
  auto queue = routing_table.find(actor_id);
  if (queue != routing_table.end()) {
    routing_table.insert(std::make_pair(alias_id, queue->second));
  }
}

void Router::deregister_actor(uint64_t actor_id) {
  std::unique_lock<std::mutex> lock(mutex);
  auto queue = routing_table.find(actor_id);
  if (queue != routing_table.end()) {
    vQueueDelete(queue->second);
    routing_table.erase(queue);
  }
}

void Router::send(uint64_t sender, uint64_t receiver, Message message) {
  // std::unique_lock<std::mutex> lock(mutex);
  auto queue = routing_table.find(receiver);
  if (queue != routing_table.end()) {
    QueueHandle_t handle = queue->second;
    // lock.unlock();
    message.sender(sender);
    message.receiver(receiver);
#if BY_VALUE
    xQueueSend(handle, message.raw(), portMAX_DELAY);
#else
    xQueueSend(handle, &message, portMAX_DELAY);
    message.moved();
#endif
  }
}

std::optional<Message> Router::receive(uint64_t receiver, size_t wait_time) {
  std::unique_lock<std::mutex> lock(mutex);
  auto queue = routing_table.find(receiver);
  if (queue != routing_table.end()) {
    QueueHandle_t handle = queue->second;
    lock.unlock();  // This is not completely safe and assumes a queue is not
                    // read after delete is called
#if BY_VALUE
    auto message = Message(STATIC_MESSAGE_SIZE);
    if (xQueueReceive(handle, message.raw(), portMAX_DELAY)) {
      return message;
    }
#else
    auto message = Message(0, false);
    if (xQueueReceive(handle, &message, wait_time)) {
      // printf("%lld -> %lld\n", message.sender(), message.receiver());
      return message;
    }
#endif
  }
  return std::nullopt;
}
#endif
