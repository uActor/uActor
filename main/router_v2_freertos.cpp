
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdio>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <variant>

#include "include/router_v2.hpp"

class RouterNode::Queue {
 public:
  Queue() { queue = xQueueCreate(QUEUE_SIZE, sizeof(Message)); }

  ~Queue() { vQueueDelete(queue); }

  void send_message(const Message& message) {
    Message m = message;
    xQueueSend(queue, &m, portMAX_DELAY);
    m.moved();
  }

  std::optional<Message> receive_message(uint32_t timeout) {
    Message message = Message();
    if (xQueueReceive(queue, &message, timeout)) {
      // printf("%s -> %s\n", message.sender(), message.receiver());
      return message;
    }
    return std::nullopt;
  }

  QueueHandle_t queue;
};

RouterNode::~RouterNode() {
  if (is_master()) {
    delete std::get<MasterStructure>(content).queue;
  }
}

void RouterNode::_add_master() {
  content.emplace<MasterStructure>(new Queue());
}

std::optional<Message> RouterNode::_receive_message(uint32_t timeout) {
  return std::get<MasterStructure>(content).queue->receive_message(timeout);
}

void RouterNode::_send_message(const Message& message) {
  std::get<MasterStructure>(content).queue->send_message(message);
}

void RouterNode::_remove_queue() {
  delete std::get<MasterStructure>(content).queue;
}
