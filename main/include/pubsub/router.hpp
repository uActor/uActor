#ifndef MAIN_INCLUDE_PUBSUB_ROUTER_HPP_
#define MAIN_INCLUDE_PUBSUB_ROUTER_HPP_

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

#include "constraint_index.hpp"
#include "filter.hpp"
#include "publication.hpp"
#include "receiver.hpp"
#include "receiver_handle.hpp"
#include "subscription.hpp"

namespace uActor::PubSub {

class Router {
 public:
  static Router& get_instance() {
    static Router instance;
    return instance;
  }

  static void os_task(void*);

  void publish(Publication&& publication);

  ReceiverHandle new_subscriber();

  std::string subscriptions_for(std::string_view node_id);

 private:
  std::atomic<uint32_t> next_sub_id{1};
  std::list<Receiver*> receivers;
  std::map<uint32_t, Subscription> subscriptions;
  std::map<std::string, ConstraintIndex> constraints;
  // Index for all subscriptions without required constraints.
  std::set<Subscription*> no_requirements;

  std::atomic<bool> updated{false};
  std::recursive_mutex mtx;

  uint32_t add_subscription(
      Filter&& f, Receiver* r,
      std::string subscriber_node_id = std::string("local"));
  uint32_t remove_subscription(uint32_t sub_id, Receiver* r,
                               std::string node_id = "");

  void deregister_receiver(Receiver* r) {
    std::unique_lock lock(mtx);
    receivers.remove(r);
  }

  void register_receiver(Receiver* r) {
    std::unique_lock lock(mtx);
    receivers.push_back(r);
  }

  friend ReceiverHandle;
  friend Receiver;

  void publish_subscription_update();
};

}  // namespace uActor::PubSub
#endif  //  MAIN_INCLUDE_PUBSUB_ROUTER_HPP_
