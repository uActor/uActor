#include <list>
#include <publication.hpp>
#include <subscription.hpp>

namespace PubSub {
class Router::Receiver::Queue {
 public:
  void send_message(MatchedPublication&& publication) {
    // printf("%s -> %s\n", message.sender(), message.receiver());
    queue.emplace_back(std::move(publication));
  }

  std::optional<MatchedPublication> receive_message(uint32_t timeout) {
    if (queue.begin() != queue.end()) {
      MatchedPublication pub = std::move(*queue.begin());
      // printf("%s -> %s\n", m.sender(), m.receiver());
      queue.pop_front();
      return std::move(pub);
    }
    return std::nullopt;
  }

  std::list<MatchedPublication> queue;
};

Router::Receiver::Receiver(Router* router) : router(router) {
  router->register_receiver(this);
  queue = std::make_unique<Router::Receiver::Queue>();
}

Router::Receiver::~Receiver() { router->deregister_receiver(this); }

std::optional<MatchedPublication> Router::Receiver::receive(uint32_t timeout) {
  return queue->receive_message(timeout);
}

void Router::Receiver::publish(MatchedPublication&& publication) {
  queue->send_message(std::move(publication));
}

SubscriptionHandle PubSub::Router::new_subscriber() {
  return SubscriptionHandle{this};
}

}  // namespace PubSub
