#ifndef UACTOR_INCLUDE_PUBSUB_RECEIVER_HPP_
#define UACTOR_INCLUDE_PUBSUB_RECEIVER_HPP_

#define RECEIVER_QUEUE_HARD_LIMIT 100

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

  static std::atomic<int> size_diff;

 private:
  Router* router;
  std::unique_ptr<Queue> queue;
  std::set<uint32_t> subscriptions;

  std::optional<MatchedPublication> receive(uint32_t wait_time = 0);

  uint32_t subscribe(Filter&& f, std::string node_id = "local");

  void unsubscribe(uint32_t sub_id, std::string node_id = "");

  void publish(MatchedPublication&& publication);

  friend ReceiverHandle;
  friend Router;
};

}  // namespace uActor::PubSub

#endif  //  UACTOR_INCLUDE_PUBSUB_RECEIVER_HPP_
