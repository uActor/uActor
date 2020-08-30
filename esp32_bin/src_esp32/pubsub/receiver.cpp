#include "pubsub/receiver.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <list>

#include "pubsub/publication.hpp"
#include "pubsub/router.hpp"

namespace uActor::PubSub {
class Receiver::Queue {
 public:
  Queue() {
    queue =
        xQueueCreate(RECEIVER_QUEUE_HARD_LIMIT, sizeof(MatchedPublication*));
  }

  ~Queue() { vQueueDelete(queue); }

  void send_message(MatchedPublication&& publication) {
    MatchedPublication* p = new MatchedPublication(std::move(publication));
    xQueueSend(queue, &p, portMAX_DELAY);
  }

  std::optional<MatchedPublication> receive_message(uint32_t timeout) {
    MatchedPublication* pub = nullptr;

    if (xQueueReceive(queue, &pub, timeout)) {
      MatchedPublication out = MatchedPublication(std::move(*pub));
      delete pub;
      return std::move(out);
    }
    return std::nullopt;
  }

  QueueHandle_t queue;
};

Receiver::Receiver(Router* router) : router(router) {
  router->register_receiver(this);
  queue = std::make_unique<Receiver::Queue>();
}

Receiver::~Receiver() {
  for (auto sub : subscriptions) {
    router->remove_subscription(sub, this);
  }
  router->deregister_receiver(this);
}

std::optional<MatchedPublication> Receiver::receive(uint32_t timeout) {
  return queue->receive_message(timeout);
}

void Receiver::publish(MatchedPublication&& publication) {
  queue->send_message(std::move(publication));
}

uint32_t Receiver::subscribe(Filter&& f, std::string node_id) {
  uint32_t sub_id = router->add_subscription(std::move(f), this, node_id);
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