#include <list>

#include "board_functions.hpp"
#include "pubsub/matched_publication.hpp"
#include "pubsub/receiver.hpp"
#include "pubsub/router.hpp"

namespace uActor::PubSub {

class Receiver::Queue {
 public:
  void send_message(MatchedPublication&& publication) {
    queue.emplace_back(std::move(publication));
  }

  std::optional<MatchedPublication> receive_message(uint32_t timeout) {
    do {
      if (queue.begin() != queue.end()) {
        MatchedPublication pub = std::move(*queue.begin());
        queue.pop_front();
        return std::move(pub);
      }
    } while (BoardFunctions::timestamp() < timeout);
    return std::nullopt;
  }

  std::list<MatchedPublication> queue;
};

Receiver::Receiver(Router* router) : router(router) {
  queue = std::make_unique<Receiver::Queue>();
}

Receiver::~Receiver() {
  for (auto sub : subscriptions) {
    router->remove_subscription(sub, this);
  }
}

std::optional<MatchedPublication> Receiver::receive(uint32_t timeout) {
  return queue->receive_message(BoardFunctions::timestamp() + timeout);
}

void Receiver::publish(MatchedPublication&& publication) {
  queue->send_message(std::move(publication));
}

uint32_t Receiver::subscribe(Filter&& f, std::string node_id, uint8_t priority) {
  uint32_t sub_id = router->add_subscription(std::move(f), this, node_id, priority);
  subscriptions.insert(sub_id);
  return sub_id;
}

void Receiver::unsubscribe(uint32_t sub_id, std::string node_id) {
  size_t sub_count = router->remove_subscription(sub_id, this, node_id);
  if (sub_count == 0) {
    subscriptions.erase(sub_id);
  }
}

}  // namespace uActor::PubSub
