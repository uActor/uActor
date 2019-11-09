
#include "managed_actor.hpp"

uint32_t ManagedActor::receive_next_internal() {
  this->receive(message_queue.front());
  message_queue.pop_front();
  if (waiting) {
    auto it = std::find_if(
        message_queue.begin(), message_queue.end(),
        [&](Message& message) { return pattern.matches(message); });
    if (it != message_queue.end()) {
      Message message = std::move(*it);
      message_queue.erase(it);
      message_queue.push_front(std::move(message));
      return 0;
    }
  } else if (message_queue.size() > 0) {
    return 0;
  }
  return _timeout;
}

bool ManagedActor::enqueue(Message&& message) {
  if (waiting && pattern.matches(message)) {
    message_queue.emplace_front(std::move(message));
    return false;
  } else {
    message_queue.emplace_back(std::move(message));
    return true;
  }
}
