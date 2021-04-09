#pragma once

#define RECEIVER_QUEUE_HARD_LIMIT 100

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif

#include <list>
#include <memory>
#include <set>
#include <string>

#include "filter.hpp"
#include "matched_publication.hpp"

namespace uActor::PubSub {

class ReceiverHandle;
class Router;

class Receiver {
 public:
  class Queue;
  explicit Receiver(Router* router);
  ~Receiver();

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  static std::atomic<int> total_queue_size;
#endif

 private:
  Router* router;
  std::unique_ptr<Queue> queue;
  std::set<uint32_t> subscriptions;

  std::optional<MatchedPublication> receive(uint32_t timeout = 0);

  uint32_t subscribe(Filter&& f, std::string node_id = "local",
                     uint8_t priority = 0);

  void unsubscribe(uint32_t sub_id, std::string node_id = "");

  void publish(MatchedPublication&& publication);

  friend ReceiverHandle;
  friend Router;
};

}  // namespace uActor::PubSub
