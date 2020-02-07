#ifndef MAIN_INCLUDE_SUBSCRIPTION_HPP_
#define MAIN_INCLUDE_SUBSCRIPTION_HPP_

#include <testbed.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <variant>

#include "board_functions.hpp"
#include "publication.hpp"
#include "string_helper.hpp"

#include "pubsub/constraint.hpp"
#include "pubsub/filter.hpp"

namespace PubSub {
class Router;
class Receiver;

struct Subscription {
  Subscription(uint32_t id, Filter f, std::string node_id, Receiver* r)
      : subscription_id(id), filter(f) {
    nodes.emplace(node_id, std::list<Receiver*>{r});
    receivers.emplace(r, std::list<std::string>{node_id});
  }

  uint32_t subscription_id;
  Filter filter;

  std::unordered_map<std::string, std::list<Receiver*>> nodes;
  std::unordered_map<Receiver*, std::list<std::string>> receivers;
};

struct MatchedPublication {
  MatchedPublication(Publication&& p, uint32_t subscription_id)
      : publication(p), subscription_id(subscription_id) {}

  MatchedPublication() : publication(), subscription_id(0) {}
  Publication publication;
  uint32_t subscription_id;
};

class SubscriptionHandle;

class Receiver {
 public:
  class Queue;
  explicit Receiver(Router* router);
  ~Receiver();

 private:
  std::optional<MatchedPublication> receive(uint32_t wait_time = 0);

  uint32_t subscribe(Filter&& f, std::string node_id = "local");

  void unsubscribe(uint32_t sub_id, std::string node_id = "");

  void publish(MatchedPublication&& publication);

 private:
  Router* router;
  std::unique_ptr<Queue> queue;

  std::set<uint32_t> subscriptions;

  friend SubscriptionHandle;
  friend Router;
};

class Router {
 public:
  static Router& get_instance() {
    static Router instance;
    return instance;
  }

  static void os_task(void*) {
    Router& router = get_instance();
    while (true) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      bool t = true;
      if (router.updated.compare_exchange_weak(t, false)) {
        Publication update{BoardFunctions::NODE_ID, "router", "1"};
        update.set_attr("type", "local_subscription_update");
        update.set_attr("node_id", BoardFunctions::NODE_ID);
        router.publish(std::move(update));
      }
    }
  }

  void publish(Publication&& publication) {
    bool subscriber_found = false;
    testbed_start_timekeeping("publish");
    std::unique_lock lock(mtx);
    for (auto& subscribtion : subscriptions) {
      if (subscribtion.second.filter.matches(publication)) {
        for (auto& receiver : subscribtion.second.receivers) {
          subscriber_found = true;
          receiver.first->publish(
              MatchedPublication(Publication(publication), subscribtion.first));
        }
      }
    }
    testbed_stop_timekeeping("publish");
    if (!subscriber_found) {
      printf("useless message\n");
    }
  }

  SubscriptionHandle new_subscriber();

  std::string subscriptions_for(std::string_view node_id) {
    std::string serialized_sub;
    for (auto& sub : subscriptions) {
      if (sub.second.nodes.size() > 2 ||
          sub.second.nodes.find(std::string(node_id)) ==
              sub.second.nodes.end()) {
        serialized_sub += sub.second.filter.serialize() + "&";
      }
    }
    return serialized_sub;
  }

 private:
  std::atomic<uint32_t> next_sub_id{1};

  std::list<Receiver*> receivers;
  std::map<uint32_t, Subscription> subscriptions;

  std::atomic<bool> updated{false};

  std::recursive_mutex mtx;

  uint32_t add_subscription(Filter&& f, Receiver* r,
                            std::string peer_node_id = std::string("local")) {
    std::unique_lock lck(mtx);
    if (auto it =
            std::find_if(subscriptions.begin(), subscriptions.end(),
                         [&](auto& sub) { return sub.second.filter == f; });
        it != subscriptions.end()) {
      Subscription& sub = it->second;

      if (auto rec_it = sub.receivers.find(r); rec_it != sub.receivers.end()) {
        if (std::find(rec_it->second.begin(), rec_it->second.end(),
                      peer_node_id) == rec_it->second.end()) {
          rec_it->second.push_back(peer_node_id);
        }
      } else {
        sub.receivers.try_emplace(
            r, std::list<std::string>{std::string(peer_node_id)});
      }

      // Check if this subscription is for a new node. A new node will affect
      // the external subscriptions.
      if (auto node_it = sub.nodes.find(peer_node_id);
          node_it != sub.nodes.end()) {
        // Additional receiver: For remote nodes this could mean there is a
        // second path.
        if (std::find(node_it->second.begin(), node_it->second.end(), r) ==
            node_it->second.end()) {
          node_it->second.push_back(r);
        }
      } else {
        if (peer_node_id == "local") {
          publish_subscription_update();
        } else if (sub.nodes.size() == 1) {
          // TODO(raphaelhetzel) Single node update optimization
          // std::string update_node_id = sub.nodes.begin()->first;
          publish_subscription_update();
        }
        sub.nodes.emplace(peer_node_id, std::list<Receiver*>{r});
      }

      return sub.subscription_id;
    } else {  // new subscription
      uint32_t id = next_sub_id++;
      if (peer_node_id == "local") {
        publish_subscription_update();
      } else {
        // TODO(raphaelhetzel) Optimization: We could exclude peer_node_id here.
        publish_subscription_update();
      }
      subscriptions.try_emplace(id, id, std::move(f), std::move(peer_node_id),
                                r);
      return id;
    }
  }

  uint32_t remove_subscription(uint32_t sub_id, Receiver* r,
                               std::string node_id = "") {
    std::unique_lock lck(mtx);
    if (auto sub_it = subscriptions.find(sub_id);
        sub_it != subscriptions.end()) {
      auto& sub = sub_it->second;
      if (auto receiver_it = sub.receivers.find(r);
          receiver_it != sub.receivers.end()) {
        auto& receiver = *receiver_it;

        if (node_id.empty()) {  // Empty node id -> remove subscription for all
                                // node_ids managed by the receiver
          for (auto& sub_node_id : receiver.second) {
            remove_subscription_for_node(&sub, r, sub_node_id);
          }
          sub.receivers.erase(receiver_it);
          return 0;
        } else {
          remove_subscription_for_node(&sub, r, node_id);
          if (receiver.second.size() == 1) {
            sub.receivers.erase(receiver_it);
          } else {
            if (auto node_id_it = std::find(receiver_it->second.begin(),
                                            receiver_it->second.end(), node_id);
                node_id_it != receiver_it->second.end()) {
              receiver_it->second.erase(node_id_it);
            }
          }
          return 0;
        }
      }
    }
    return 0;
  }

  void remove_subscription_for_node(Subscription* sub, Receiver* r,
                                    std::string node_id) {
    if (auto node_it = sub->nodes.find(node_id); node_it != sub->nodes.end()) {
      if (node_it->second.size() == 1) {
        sub->nodes.erase(node_it);
        if (sub->nodes.size() == 0) {
          // TODO(raphaelhetzel) optimization: potentially delay this longer
          // than subscriptions
          publish_subscription_update();
        } else if (sub->nodes.size() == 1) {
          if (node_id != "local") {
            // TODO(raphaelhetzel) optimization: only send to a single node
            publish_subscription_update();
          }
        }
      } else {
        if (auto reference_it =
                std::find(node_it->second.begin(), node_it->second.end(), r);
            reference_it != node_it->second.end()) {
          node_it->second.erase(reference_it);
        }
      }
    }
  }

  void publish_subscription_update() { updated = true; }

  void deregister_receiver(Receiver* r) {
    std::unique_lock lock(mtx);
    receivers.remove(r);
  }
  void register_receiver(Receiver* r) {
    std::unique_lock lock(mtx);
    receivers.push_back(r);
  }
  friend SubscriptionHandle;
  friend Receiver;
};

class SubscriptionHandle {
 public:
  explicit SubscriptionHandle(Router* router)
      : receiver(std::make_unique<Receiver>(router)) {}

  uint32_t subscribe(Filter f,
                     std::string_view node_id = std::string_view("local")) {
    return receiver->subscribe(std::move(f), std::string(node_id));
  }

  void unsubscribe(uint32_t sub_id,
                   std::string_view node_id = std::string_view("")) {
    receiver->unsubscribe(sub_id, std::string(node_id));
  }

  std::optional<MatchedPublication> receive(uint32_t wait_time) {
    return receiver->receive(wait_time);
  }

 private:
  std::unique_ptr<Receiver> receiver;
};
}  // namespace PubSub

#endif  //  MAIN_INCLUDE_SUBSCRIPTION_HPP_
