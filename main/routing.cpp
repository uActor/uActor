#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <list>

#include "benchmark_configuration.hpp"
#include "include/publication.hpp"
#include "include/subscription.hpp"
namespace PubSub {
class Router::Receiver::Queue {
 public:
  Queue() { queue = xQueueCreate(QUEUE_SIZE, sizeof(MatchedPublication*)); }

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

Router::Receiver::Receiver(Router* router) : router(router) {
  router->register_receiver(this);
  queue = std::make_unique<Receiver::Queue>();
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
