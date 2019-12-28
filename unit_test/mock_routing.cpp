#include <list>
#include <publication.hpp>
#include <subscription.hpp>

class RouterV3::Receiver::Queue {
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

RouterV3::Receiver::Receiver(RouterV3* router, std::string node_id,
                             std::string actor_type, std::string instance_id)
    : router(router) {
  router->register_receiver(this);
  queue = std::make_unique<RouterV3::Receiver::Queue>();
  Filter f = Filter{Constraint(std::string("node_id"), node_id),
                    Constraint(std::string("actor_type"), actor_type),
                    Constraint(std::string("instance_id"), instance_id)};
  _primary_subscription_id = router->next_sub_id++;
  filters.push_back(std::make_pair(_primary_subscription_id, std::move(f)));
}

RouterV3::Receiver::~Receiver() { router->deregister_receiver(this); }

std::optional<RouterV3::MatchedPublication> RouterV3::Receiver::receive(
    size_t timeout) {
  return queue->receive_message(timeout);
}

void RouterV3::Receiver::publish(MatchedPublication&& publication) {
  queue->send_message(std::move(publication));
}
