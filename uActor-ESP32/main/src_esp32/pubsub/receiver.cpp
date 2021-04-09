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

  ~Queue() {
#if CONFIG_UACTOR_ENABLE_TELEMETRY
    Receiver::total_queue_size -= uxQueueMessagesWaiting(queue);
#endif
    vQueueDelete(queue);
  }

  void send_message(MatchedPublication&& publication) {
    MatchedPublication* p = new MatchedPublication(std::move(publication));
    xQueueSend(queue, &p, portMAX_DELAY);
#if CONFIG_UACTOR_ENABLE_TELEMETRY
    Receiver::total_queue_size++;
#endif
  }

  std::optional<MatchedPublication> receive_message(uint32_t timeout) {
    MatchedPublication* pub = nullptr;

    if (xQueueReceive(queue, &pub, timeout)) {
      MatchedPublication out = MatchedPublication(std::move(*pub));
      delete pub;
#if CONFIG_UACTOR_ENABLE_TELEMETRY
      Receiver::total_queue_size--;
#endif
      return std::move(out);
    }
    return std::nullopt;
  }

  QueueHandle_t queue;
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
  return queue->receive_message(timeout);
}

void Receiver::publish(MatchedPublication&& publication) {
  queue->send_message(std::move(publication));
}

uint32_t Receiver::subscribe(Filter&& f, std::string node_id,
                             uint8_t priority) {
  uint32_t sub_id =
      router->add_subscription(std::move(f), this, node_id, priority);
  subscriptions.insert(sub_id);
  return sub_id;
}

void Receiver::unsubscribe(uint32_t sub_id, std::string node_id) {
  size_t sub_count = router->remove_subscription(sub_id, this, node_id);
  if (sub_count == 0) {
    subscriptions.erase(sub_id);
  }
}

#if CONFIG_UACTOR_ENABLE_TELEMETRY
std::atomic<int> Receiver::total_queue_size{0};
#endif

}  // namespace uActor::PubSub
