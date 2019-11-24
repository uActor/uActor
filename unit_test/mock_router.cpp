#include <cstdio>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <router_v2.hpp>

class RouterNode::Queue {
 public:
  void send_message(const Message& message) {
    printf("%s -> %s\n", message.sender(), message.receiver());
    Message m = message;
    queue.emplace_back(std::move(m));
  }

  std::optional<Message> receive_message(uint32_t timeout) {
    if (queue.begin() != queue.end()) {
      Message m = std::move(*queue.begin());
      printf("%s -> %s\n", m.sender(), m.receiver());
      queue.pop_front();
      return std::move(m);
    }
    return std::nullopt;
  }

  std::list<Message> queue;
};

RouterNode::~RouterNode() {
  if (is_master()) {
    delete std::get<MasterStructure>(content).queue;
  }
}

void RouterNode::_add_master() {
  printf("add_master\n");
  content.emplace<MasterStructure>(new Queue());
}

std::optional<Message> RouterNode::_receive_message(uint32_t timeout) {
  printf("receive %d\n", std::holds_alternative<MasterStructure>(content));
  return std::get<MasterStructure>(content).queue->receive_message(timeout);
}

void RouterNode::_send_message(const Message& message) {
  printf("wrapper %d\n", std::holds_alternative<MasterStructure>(content));
  std::get<MasterStructure>(content).queue->send_message(message);
}

void RouterNode::_remove_queue() {
  delete std::get<MasterStructure>(content).queue;
}
