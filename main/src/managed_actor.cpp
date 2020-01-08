#include "managed_actor.hpp"

#define ACTOR_QUEUE_SOFTLIMIT 100

ManagedActor::ReceiveResult ManagedActor::receive_next_internal() {
  waiting = false;
  _timeout = UINT32_MAX;
  pattern.filter.clear();

  const auto& next_message = message_queue.front();
  this->receive(message_queue.front());  // Exit message is processed to allow
                                         // for any necessary cleanup
  if (next_message.has_attr("type") &&
      std::get<std::string_view>(next_message.get_attr("type")) == "exit") {
    return ManagedActor::ReceiveResult(true, 0);
  }
  message_queue.pop_front();
  if (waiting) {
    auto it = std::find_if(
        message_queue.begin(), message_queue.end(),
        [&](Publication& message) { return pattern.matches(message); });
    if (it != message_queue.end()) {
      Publication message = std::move(*it);
      message_queue.erase(it);
      message_queue.push_front(std::move(message));
      return ManagedActor::ReceiveResult(false, 0);
    }
  } else if (message_queue.size() > 0) {
    return ManagedActor::ReceiveResult(false, 0);
  }
  return ManagedActor::ReceiveResult(false, _timeout);
}

bool ManagedActor::enqueue(Publication&& pub) {
  if (message_queue.size() >= ACTOR_QUEUE_SOFTLIMIT) {
    printf("Warning: Actor queue size excedes configured limit.");
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
