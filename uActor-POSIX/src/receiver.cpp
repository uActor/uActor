#include "pubsub/receiver.hpp"

#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>

#include "board_functions.hpp"
#include "pubsub/matched_publication.hpp"
#include "pubsub/router.hpp"

namespace uActor::PubSub {

class Receiver::Queue {
 public:
#if CONFIG_UACTOR_ENABLE_TELEMETRY
  ~Queue() { Receiver::total_queue_size -= queue.size(); }
#endif

  void send_message(MatchedPublication&& publication) {
    std::unique_lock lock(mtx);
    auto was_empty = queue.begin() == queue.end();
    queue.emplace_back(std::move(publication));
#if CONFIG_UACTOR_ENABLE_TELEMETRY
    Receiver::total_queue_size++;
#endif
    if (was_empty) {
      queue_cv.notify_one();
    }
  }

  std::optional<MatchedPublication> receive_message(uint32_t timeout) {
    std::unique_lock lock(mtx);

    queue_cv.wait_for(lock, std::chrono::milliseconds(timeout),
                      [&]() { return queue.begin() != queue.end(); });
    if (queue.begin() != queue.end()) {
      MatchedPublication pub = std::move(*queue.begin());
      queue.pop_front();
#if CONFIG_UACTOR_ENABLE_TELEMETRY
      Receiver::total_queue_size--;
#endif
      return std::move(pub);
    } else {
      return std::nullopt;
    }
  }

  std::list<MatchedPublication> queue;
  std::mutex mtx;
  std::condition_variable queue_cv;
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

uint32_t Receiver::subscribe(Filter&& f, const ActorIdentifier& subscriber,
                             const std::string& source_peer,
                             SubscriptionArguments arguments) {
  uint32_t sub_id = router->add_subscription(std::move(f), this, subscriber,
                                             source_peer, arguments);
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
