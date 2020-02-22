#include "pubsub/router.hpp"

#include <utility>

#include "board_functions.hpp"
#include "pubsub/constraint.hpp"
#include "pubsub/matched_publication.hpp"
#include "remote_connection.hpp"

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
//         receiver.first->publish(
//             MatchedPublication(Publication(publication),
//             subscribtion.first));
//       }
//     }
//   }
// }

void Router::publish(Publication&& publication) {
  Counter c;
  std::unique_lock lock(mtx);

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
  std::unique_lock lock(mtx);
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
  if (auto it =
          std::find_if(subscriptions.begin(), subscriptions.end(),
                       [&](const auto& sub) { return sub.second.filter == f; });
      it != subscriptions.end()) {
    Subscription& sub = it->second;
    sub.add_receiver(r, subscriber_node_id);
    if (sub.nodes.size() == 2 ||
        (sub.nodes.size() == 1 && sub.nodes.begin()->first == "local")) {
      publish_subscription_update();
    }
    return sub.subscription_id;
  } else {  // new subscription
    uint32_t id = next_sub_id++;

    auto [sub_it, inserted] = subscriptions.try_emplace(
        id, id, std::move(f), std::move(subscriber_node_id), r);

    // Index maintenance
    if (inserted && sub_it->second.count_required == 0) {
      no_requirements.insert(&sub_it->second);
    }
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

    // TODO(raphaelhetzel) Optimization: We could exclude peer_node_id here.
    publish_subscription_update();

    return id;
  }
}

uint32_t Router::remove_subscription(uint32_t sub_id, Receiver* r,
                                     std::string node_id) {
  size_t remaining = 0;
  std::unique_lock lck(mtx);
  if (auto sub_it = subscriptions.find(sub_id); sub_it != subscriptions.end()) {
    auto& sub = sub_it->second;
    auto [empty_sub, rem] = sub.remove_receiver(r, node_id);
    remaining = rem;
    if (empty_sub) {
      // Index maintenance
      if (sub.count_required == 0) {
        no_requirements.erase(&sub);
      }

      for (auto constraint : sub.filter.required) {
        if (auto constraint_it =
                constraints.find(std::string(constraint.attribute()));
            constraint_it != constraints.end()) {
          if (constraint_it->second.remove(constraint, &sub)) {
            constraints.erase(constraint_it);
          }
        }
      }

      for (auto constraint : sub.filter.optional) {
        if (auto constraint_it =
                constraints.find(std::string(constraint.attribute()));
            constraint_it != constraints.end()) {
          if (constraint_it->second.remove(constraint, &sub)) {
            constraints.erase(constraint_it);
          }
        }
      }

      subscriptions.erase(sub.subscription_id);
      // TODO(raphaelhetzel) optimization: potentially delay this longer
      // than subscriptions
      publish_subscription_update();
    } else {
      // if( sub.nodes.size() == 1) {
      // TODO(raphaelhetzel) optimization: only send to a single node
      publish_subscription_update();
      // }
    }
  }
  return remaining;
}

void Router::publish_subscription_update() { updated = true; }

ReceiverHandle Router::new_subscriber() { return ReceiverHandle{this}; }
}  // namespace uActor::PubSub
