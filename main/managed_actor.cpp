#include "include/managed_actor.hpp"

#include "benchmark_configuration.hpp"
uint32_t ManagedActor::receive_next_internal() {
  waiting = false;
  _timeout = UINT32_MAX;
  pattern.filter.clear();

  this->receive(message_queue.front());
  message_queue.pop_front();
  if (waiting) {
    auto it = std::find_if(
        message_queue.begin(), message_queue.end(),
        [&](Publication& message) { return pattern.matches(message); });
    if (it != message_queue.end()) {
      Publication message = std::move(*it);
      message_queue.erase(it);
      message_queue.push_front(std::move(message));
      return 0;
    }
  } else if (message_queue.size() > 0) {
    return 0;
  }
  return _timeout;
}

bool ManagedActor::enqueue(Publication&& pub) {
  if (message_queue.size() >= QUEUE_SIZE) {
    printf("Warning: Actor queue size excedes configured maximum.");
  }
  if (waiting) {
    if (pattern.matches(pub)) {
      message_queue.push_front(std::move(pub));
      return true;
    } else {
      message_queue.push_back(std::move(pub));
      return false;
    }
  } else {
    message_queue.push_back(std::move(pub));
    return true;
  }
}
