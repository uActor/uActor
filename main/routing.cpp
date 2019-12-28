#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <list>

#include "benchmark_configuration.hpp"
#include "include/publication.hpp"
#include "include/subscription.hpp"
class RouterV3::Receiver::Queue {
 public:
  Queue() {
    printf("constructor\n");
    queue = xQueueCreate(QUEUE_SIZE, sizeof(MatchedPublication*));
  }

  ~Queue() { vQueueDelete(queue); }

  void send_message(MatchedPublication&& publication) {
    MatchedPublication* p = new MatchedPublication(publication);
    xQueueSend(queue, &p, portMAX_DELAY);
  }

  std::optional<MatchedPublication> receive_message(uint32_t timeout) {
    MatchedPublication* pub = nullptr;

    if (xQueueReceive(queue, &pub, timeout)) {
      MatchedPublication out = MatchedPublication(*pub);
      delete pub;
      return std::move(out);
    }
    return std::nullopt;
  }

  QueueHandle_t queue;
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
