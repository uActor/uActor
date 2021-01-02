#include "pubsub/router.hpp"

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif
#if CONFIG_BENCHMARK_BREAKDOWN
#include <support/testbed.h>
#endif

#include <list>
#include <unordered_set>
#include <utility>

#include "board_functions.hpp"
#include "pubsub/constraint.hpp"
#include "pubsub/matched_publication.hpp"
#include "remote/sequence_number_forwarding_strategy.hpp"

namespace uActor::PubSub {

void Router::publish(Publication&& publication) {
  std::shared_lock lock(mtx);
  publish_internal(std::move(publication));
}

void Router::publish_internal(Publication&& publication) {
  Counter<Allocator, AllocatorConfiguration> c;
  std::list<std::pair<Receiver*, uint32_t>> current_receivers;
#if CONFIG_BENCHMARK_BREAKDOWN
  testbed_start_timekeeping(2);
#endif

  if (publication.get_str_attr("publisher_node_id") ==
          BoardFunctions::NODE_ID &&
      !publication.has_attr("_internal_sequence_number")) {
    int32_t seq = static_cast<int32_t>(
        Remote::SequenceNumberForwardingStrategy::sequence_number++);
    publication.set_attr("_internal_sequence_number", seq);
    publication.set_attr("_internal_epoch", BoardFunctions::epoch);
  }
#if CONFIG_BENCHMARK_BREAKDOWN
  testbed_start_timekeeping(7);
#endif
  for (const auto& [attribute, value] : publication) {
    if (auto constraint_it = constraints.find(attribute);
        constraint_it != constraints.end()) {
      constraint_it->second.check(value, &c);
    }
  }
#if CONFIG_BENCHMARK_BREAKDOWN
  if (publication.get_str_attr("type") == "ping") {
    testbed_stop_timekeeping_inner(7, "search");
  }
  testbed_start_timekeeping(7);
#endif
  for (const auto& [subscription_ptr, counts] : c) {
    auto& subscription = *subscription_ptr;
    if (counts.required == subscription.count_required) {
      if (counts.optional == subscription.count_optional ||
          subscription.filter.check_optionals(publication)) {
        for (auto& receiver : subscription.receivers) {
          current_receivers.emplace_back(receiver.first,
                                         subscription.subscription_id);
        }
      }
    }
  }

  // Check remaining subscriptions that might still match
  for (const auto& sub : no_requirements) {
    if (c.find(sub) == c.end()) {
      if (sub->filter.check_optionals(publication)) {
        for (auto& receiver : sub->receivers) {
          current_receivers.emplace_back(receiver.first, sub->subscription_id);
        }
      }
    }
  }
#if CONFIG_BENCHMARK_BREAKDOWN
  bool is_type = publication.get_str_attr("type") == "ping";
#endif
  auto rec_it = current_receivers.begin();
  for (; rec_it != std::prev(current_receivers.end()); ++rec_it) {
    rec_it->first->publish(
        MatchedPublication(Publication(publication), rec_it->second));
  }
  if (rec_it != current_receivers.end()) {
    rec_it->first->publish(
        MatchedPublication(std::move(publication), rec_it->second));
  }
#if CONFIG_BENCHMARK_BREAKDOWN
  if (is_type) {
    testbed_stop_timekeeping_inner(7, "bcast");
    testbed_stop_timekeeping_inner(2, "publish");
    testbed_start_timekeeping(6);
  }
#endif
}

std::string Router::subscriptions_for(std::string_view node_id) {
  std::shared_lock lock(mtx);
  std::string serialized_sub;
  bool send_node_id = false;

  for (auto& sub_pair : subscriptions) {
    auto& sub = sub_pair.second;
    bool skip = false;
    // Only subscriptions that could be fulfilled
    if (sub.nodes.size() >= 2 || sub.nodes.find(node_id) == sub.nodes.end()) {
      for (const auto& constraint : sub.filter.required) {
        // Don't forward local subscriptions
        if (constraint.attribute() == "publisher_node_id") {
          if (std::holds_alternative<std::string>(constraint.operand()) &&
              std::get<std::string>(constraint.operand()) ==
                  BoardFunctions::NODE_ID) {
            skip = true;
          }
        }
        // Overapproximate node_id based subscriptions as they would otherwise
        // floud the routing tables
        if (constraint.attribute() == "node_id" &&
            std::get<std::string>(constraint.operand()) ==
                BoardFunctions::NODE_ID) {
          send_node_id = true;
          skip = true;
        }
      }
      if (!skip) {
        serialized_sub += sub.filter.serialize() + "&";
      }
    }
  }

  if (send_node_id) {
    serialized_sub +=
        Filter{Constraint{"node_id", BoardFunctions::NODE_ID}}.serialize() +
        "&";
    node_id_sent_to.insert(AString(node_id, make_allocator<AString>()));
  }

  if (!serialized_sub.empty()) {
    serialized_sub.resize(serialized_sub.size() - 1);
  }
  return serialized_sub;
}

uint32_t Router::find_sub_id(Filter&& filter) {
  std::shared_lock lck(mtx);
  if (auto it = std::find_if(
          subscriptions.begin(), subscriptions.end(),
          [&](const auto& sub) { return sub.second.filter == filter; });
      it != subscriptions.end()) {
    return it->first;
  }
  return 0;
}

uint32_t Router::number_of_subscriptions() { return subscriptions.size(); }

uint32_t Router::add_subscription(Filter&& f, Receiver* r,
                                  std::string subscriber_node_id) {
  std::unique_lock lck(mtx);
  if (auto it =
          std::find_if(subscriptions.begin(), subscriptions.end(),
                       [&](const auto& sub) { return sub.second.filter == f; });
      it != subscriptions.end()) {
    Subscription<Allocator, AllocatorConfiguration>& sub = it->second;
    bool updated = sub.add_receiver(
        r, AString(subscriber_node_id, make_allocator<AString>()));
    if (updated) {
      if (sub.nodes.size() == 2) {
        std::string other_sub{};
        for (const auto& n : sub.nodes) {
          if (n.first != AString(subscriber_node_id)) {
            other_sub = n.first;
          }
        }
        publish_subscription_added(
            sub.filter, "", AString(other_sub, make_allocator<AString>()));
      }
    }
    return sub.subscription_id;
  } else {  // new subscription
    uint32_t id = next_sub_id++;
    auto [sub_it, inserted] = subscriptions.try_emplace(
        id, id, std::move(f),
        AString(subscriber_node_id, make_allocator<AString>()), r);

    // Index maintenance
    if (inserted && sub_it->second.count_required == 0) {
      no_requirements.insert(&sub_it->second);
    }
    for (auto constraint : sub_it->second.filter.required) {
      auto [constraint_it, _inserted] = constraints.try_emplace(
          AString(constraint.attribute(), make_allocator<AString>()),
          ConstraintIndex<Allocator, AllocatorConfiguration>());
      constraint_it->second.insert(std::move(constraint), &(sub_it->second),
                                   false);
    }
    for (auto constraint : sub_it->second.filter.optional) {
      auto [constraint_it, _inserted] = constraints.try_emplace(
          AString(constraint.attribute(), make_allocator<AString>()),
          ConstraintIndex<Allocator, AllocatorConfiguration>());
      constraint_it->second.insert(std::move(constraint), &(sub_it->second),
                                   true);
    }

    publish_subscription_added(
        sub_it->second.filter,
        AString(subscriber_node_id, make_allocator<AString>()), "");
    return id;
  }
}

uint32_t Router::remove_subscription(uint32_t sub_id, Receiver* r,
                                     std::string node_id) {
  size_t remaining = 0;
  std::unique_lock lck(mtx);
  if (auto sub_it = subscriptions.find(sub_id); sub_it != subscriptions.end()) {
    auto& sub = sub_it->second;
    auto [empty_sub, rem] = sub.remove_receiver(r, AString(node_id));
    remaining = rem;
    if (empty_sub) {
      // Index maintenance
      if (sub.count_required == 0) {
        no_requirements.erase(&sub);
      }

      for (const auto& constraint : sub.filter.required) {
        if (auto constraint_it = constraints.find(constraint.attribute());
            constraint_it != constraints.end()) {
          if (constraint_it->second.remove(constraint, &sub)) {
            constraints.erase(constraint_it);
          }
        }
      }

      for (const auto& constraint : sub.filter.optional) {
        if (auto constraint_it = constraints.find(constraint.attribute());
            constraint_it != constraints.end()) {
          if (constraint_it->second.remove(constraint, &sub)) {
            constraints.erase(constraint_it);
          }
        }
      }

      Filter f = std::move(sub.filter);
      subscriptions.erase(sub.subscription_id);
      publish_subscription_removed(f, "", "");
    } else {
      if (sub.nodes.size() == 1 && sub.nodes.begin()->first != "local") {
        publish_subscription_removed(
            sub.filter, "",
            AString(sub.nodes.begin()->first, make_allocator<AString>()));
      }
    }
  }
  return remaining;
}

void Router::publish_subscription_update() { updated = true; }

void Router::publish_subscription_added(const Filter& filter, AString exclude,
                                        AString include) {
  std::string serialized = filter.serialize();

  // Sub optimization
  for (const auto& constraint : filter.required) {
    if (constraint.attribute() == "publisher_node_id") {
      if (std::holds_alternative<std::string>(constraint.operand()) &&
          std::get<std::string>(constraint.operand()) ==
              BoardFunctions::NODE_ID) {
        include = "local";
      }
    }
    // Overapproximate node_id
    if (constraint.attribute() == "node_id" &&
        std::get<std::string>(constraint.operand()) ==
            BoardFunctions::NODE_ID) {
      for (const auto& node : peer_node_ids) {
        if (node != exclude &&
            node_id_sent_to.find(node) == node_id_sent_to.end()) {
          Publication update{BoardFunctions::NODE_ID, "router", "1"};
          update.set_attr("type", "subscription_added");
          update.set_attr("serialized_subscription",
                          Filter{Constraint{"node_id", BoardFunctions::NODE_ID}}
                              .serialize());
          update.set_attr("include_node_id", node);
          publish_internal(std::move(update));
          node_id_sent_to.insert(node);
        }
      }
      return;
    }
  }

  Publication update{BoardFunctions::NODE_ID, "router", "1"};
  update.set_attr("type", "subscription_added");
  update.set_attr("serialized_subscription", std::move(serialized));
  if (exclude.length() != 0) {
    update.set_attr("exclude_node_id", exclude);
  }
  if (include.length() != 0) {
    update.set_attr("include_node_id", include);
  }
  publish_internal(std::move(update));
}

void Router::publish_subscription_removed(const Filter& filter, AString exclude,
                                          AString include) {
  // Sub optimization
  for (const auto& constraint : filter.required) {
    if (constraint.attribute() == "publisher_node_id") {
      if (std::holds_alternative<std::string>(constraint.operand()) &&
          std::get<std::string>(constraint.operand()) ==
              BoardFunctions::NODE_ID) {
        include = "local";
      }
    }
    // TODO(raphaelhetzel) Keep track of the published node ids and remove them
    // as required
    if (constraint.attribute() == "node_id" &&
        std::get<std::string>(constraint.operand()) ==
            BoardFunctions::NODE_ID) {
      return;
    }
  }

  Publication update{BoardFunctions::NODE_ID, "router", "1"};
  update.set_attr("type", "subscription_removed");
  update.set_attr("serialized_subscription", filter.serialize());
  if (exclude.length() != 0) {
    update.set_attr("exclude_node_id", exclude);
  }
  if (include.length() != 0) {
    update.set_attr("include_node_id", include);
  }
  publish_internal(std::move(update));
}

ReceiverHandle Router::new_subscriber() { return ReceiverHandle{this}; }

void Router::add_peer(std::string node_id) {
  peer_node_ids.insert(AString(node_id, make_allocator<AString>()));
}
void Router::remove_peer(std::string node_id) {
  peer_node_ids.erase(AString(node_id, make_allocator<AString>()));
  node_id_sent_to.erase(AString(node_id, make_allocator<AString>()));
}
}  // namespace uActor::PubSub
