#include "pubsub/router.hpp"

#include <utility>

#include "board_functions.hpp"
#include "pubsub/constraint.hpp"
#include "pubsub/matched_publication.hpp"

namespace uActor::PubSub {

void Router::os_task(void*) {
  Router& router = get_instance();
  while (true) {
    BoardFunctions::sleep(500);
    bool t = true;
    if (router.updated.compare_exchange_weak(t, false)) {
      Publication update{BoardFunctions::NODE_ID, "router", "1"};
      update.set_attr("type", "local_subscription_update");
      update.set_attr("node_id", BoardFunctions::NODE_ID);
      router.publish(std::move(update));
    }
  }
}

// Linear scan
// void Router::publish(Publication&& publication) {
//   std::unique_lock lock(mtx);
//   for (auto& subscribtion : subscriptions) {
//     if (subscribtion.second.filter.matches(publication)) {
//       for (auto& receiver : subscribtion.second.receivers) {
//         receiver.first->publish(MatchedPublication(Publication(publication),
//         subscribtion.first));
//       }
//     }
//   }
// }

void Router::publish(Publication&& publication) {
  Counter c;

  // testbed_start_timekeeping("search");

  for (const auto& [attribute, value] : publication) {
    if (auto constraint_it = constraints.find(attribute);
        constraint_it != constraints.end()) {
      constraint_it->second.check(value, &c);
    }
  }

  // testbed_stop_timekeeping("search");
  // testbed_start_timekeeping("match");

  for (const auto& [subscription_ptr, counts] : c) {
    auto& subscription = *subscription_ptr;
    if (counts.required == subscription.count_required) {
      if (counts.optional == subscription.count_optional ||
          subscription.filter.check_optionals(publication)) {
        for (auto& receiver : subscription.receivers) {
          receiver.first->publish(MatchedPublication(
              Publication(publication), subscription.subscription_id));
        }
      }
    }
  }

  // Check remaining subscriptions that might still match
  for (auto sub : no_requirements) {
    if (c.find(sub) == c.end()) {
      if (sub->filter.check_optionals(publication)) {
        for (auto& receiver : sub->receivers) {
          receiver.first->publish(MatchedPublication(Publication(publication),
                                                     sub->subscription_id));
        }
      }
    }
  }

  // testbed_stop_timekeeping("match");
}

std::string Router::subscriptions_for(std::string_view node_id) {
  std::string serialized_sub;
  for (auto& sub : subscriptions) {
    if (sub.second.nodes.size() > 2 ||
        sub.second.nodes.find(std::string(node_id)) == sub.second.nodes.end()) {
      serialized_sub += sub.second.filter.serialize() + "&";
    }
  }
  return serialized_sub;
}

uint32_t Router::add_subscription(Filter&& f, Receiver* r,
                                  std::string subscriber_node_id) {
  std::unique_lock lck(mtx);
  if (auto it = std::find_if(subscriptions.begin(), subscriptions.end(),
                             [&](auto& sub) { return sub.second.filter == f; });
      it != subscriptions.end()) {
    Subscription& sub = it->second;

    if (auto rec_it = sub.receivers.find(r); rec_it != sub.receivers.end()) {
      if (std::find(rec_it->second.begin(), rec_it->second.end(),
                    subscriber_node_id) == rec_it->second.end()) {
        rec_it->second.push_back(subscriber_node_id);
      }
    } else {
      sub.receivers.try_emplace(
          r, std::list<std::string>{std::string(subscriber_node_id)});
    }

    // Check if this subscription is for a new node. A new node will affect
    // the external subscriptions.
    if (auto node_it = sub.nodes.find(subscriber_node_id);
        node_it != sub.nodes.end()) {
      // Additional receiver: For remote nodes this could mean there is a
      // second path.
      if (std::find(node_it->second.begin(), node_it->second.end(), r) ==
          node_it->second.end()) {
        node_it->second.push_back(r);
      }
    } else {
      if (subscriber_node_id == "local") {
        publish_subscription_update();
      } else if (sub.nodes.size() == 1) {
        // TODO(raphaelhetzel) Single node update optimization
        // std::string update_node_id = sub.nodes.begin()->first;
        publish_subscription_update();
      }
      sub.nodes.emplace(subscriber_node_id, std::list<Receiver*>{r});
    }

    return sub.subscription_id;
  } else {  // new subscription
    uint32_t id = next_sub_id++;
    if (subscriber_node_id == "local") {
      publish_subscription_update();
    } else {
      // TODO(raphaelhetzel) Optimization: We could exclude peer_node_id here.
      publish_subscription_update();
    }
    auto [sub_it, inserted] = subscriptions.try_emplace(
        id, id, std::move(f), std::move(subscriber_node_id), r);

    // Index maintenance
    if (inserted && sub_it->second.count_required == 0) {
      no_requirements.insert(&sub_it->second);
    }
    if (inserted) {
      for (auto constraint : sub_it->second.filter.required) {
        auto [constraint_it, _inserted] = constraints.try_emplace(
            std::string(constraint.attribute()), ConstraintIndex());
        constraint_it->second.insert(std::move(constraint), &(sub_it->second),
                                     false);
      }
      for (auto constraint : sub_it->second.filter.optional) {
        auto [constraint_it, _inserted] = constraints.try_emplace(
            std::string(constraint.attribute()), ConstraintIndex());
        constraint_it->second.insert(std::move(constraint), &(sub_it->second),
                                     true);
      }
    }

    return id;
  }
}

uint32_t Router::remove_subscription(uint32_t sub_id, Receiver* r,
                                     std::string node_id) {
  size_t remaining = 0;
  std::unique_lock lck(mtx);
  if (auto sub_it = subscriptions.find(sub_id); sub_it != subscriptions.end()) {
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
      } else {
        remove_subscription_for_node(&sub, r, node_id);
        if (receiver.second.size() == 1) {
          sub.receivers.erase(receiver_it);
        } else {
          if (auto node_id_it = std::find(receiver_it->second.begin(),
                                          receiver_it->second.end(), node_id);
              node_id_it != receiver_it->second.end()) {
            receiver_it->second.erase(node_id_it);
            remaining = receiver_it->second.size();
          }
        }
      }

      if (sub.nodes.size() == 0) {
        // Index maintenance
        if (sub.count_required == 0) {
          no_requirements.erase(&sub);
        }
        for (auto constraint : sub.filter.required) {
          if (auto constraint_it =
                  constraints.find(std::string(constraint.attribute()));
              constraint_it != constraints.end()) {
            if (constraint_it->second.remove(std::move(constraint), &sub)) {
              constraints.erase(constraint_it);
            }
          }
        }
        for (auto constraint : sub.filter.optional) {
          if (auto constraint_it =
                  constraints.find(std::string(constraint.attribute()));
              constraint_it != constraints.end()) {
            if (constraint_it->second.remove(std::move(constraint), &sub)) {
              constraints.erase(constraint_it);
            }
          }
        }

        subscriptions.erase(sub.subscription_id);

        // TODO(raphaelhetzel) optimization: potentially delay this longer
        // than subscriptions
        publish_subscription_update();
      } else if (sub.nodes.size() == 1) {
        if (node_id != "local") {
          // TODO(raphaelhetzel) optimization: only send to a single node
          publish_subscription_update();
        }
      }
    }
  }
  return remaining;
}

void Router::remove_subscription_for_node(Subscription* sub, Receiver* r,
                                          std::string node_id) {
  if (auto node_it = sub->nodes.find(node_id); node_it != sub->nodes.end()) {
    if (node_it->second.size() == 1) {
      sub->nodes.erase(node_it);
    } else {
      if (auto reference_it =
              std::find(node_it->second.begin(), node_it->second.end(), r);
          reference_it != node_it->second.end()) {
        node_it->second.erase(reference_it);
      }
    }
  }
}

void Router::publish_subscription_update() { updated = true; }

ReceiverHandle Router::new_subscriber() { return ReceiverHandle{this}; }
}  // namespace uActor::PubSub
