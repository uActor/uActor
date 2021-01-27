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
#include <vector>

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
  Counter<Allocator> c;
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
      if (std::holds_alternative<Publication::AString>(value)) {
        constraint_it->second.check(
            std::string_view(std::get<Publication::AString>(value)), &c);
      } else if (std::holds_alternative<int32_t>(value)) {
        constraint_it->second.check(std::get<int32_t>(value), &c);
      } else if (std::holds_alternative<float>(value)) {
        constraint_it->second.check(std::get<float>(value), &c);
      }
    }
  }
#if CONFIG_BENCHMARK_BREAKDOWN
  if (publication.get_str_attr("type") == "ping") {
    testbed_stop_timekeeping_inner(7, "search");
  }
  testbed_start_timekeeping(7);
#endif
  for (const auto& [subscription_ptr, counts] : c) {
    if (counts.required == subscription_ptr->count_required()) {
      if (counts.optional == subscription_ptr->count_optional() ||
          subscription_ptr->filter.check_optionals(publication)) {
        for (auto& receiver : subscription_ptr->receivers) {
          current_receivers.emplace_back(
              receiver.first, subscription_ptr->subscription_id);
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

std::vector<std::string> Router::subscriptions_for(std::string_view node_id,
                                                   uint32_t max_size) {
  std::shared_lock lock(mtx);
  std::vector<std::string> serialized_sub{std::string()};
  bool send_node_id = false;

  auto [peer_node_it, _inserted] =
      peer_node_ids.try_emplace(AString(node_id), false, next_node_id++);
  if (peer_node_it == peer_node_ids.end()) {
    return serialized_sub;
  }
  uint32_t internal_node_id = peer_node_it->second.second;

  for (auto& sub_pair : subscriptions) {
    auto& sub = sub_pair.second;
    bool skip = false;
    // Only subscriptions that could be fulfilled
    auto n = sub.nodes();
    if (n.size() >= 2 || n.find(internal_node_id) == n.end()) {
      for (const auto& constraint_ptr : sub.filter.required) {
        // Don't forward local subscriptions
        if (constraint_ptr->attribute() == "publisher_node_id") {
          if (std::holds_alternative<std::string_view>(
                  constraint_ptr->operand()) &&
              std::get<std::string_view>(constraint_ptr->operand()) ==
                  BoardFunctions::NODE_ID) {
            skip = true;
          }
        }
        // Overapproximate node_id based subscriptions as they would otherwise
        // floud the routing tables
        if (constraint_ptr->attribute() == "node_id" &&
            std::get<std::string_view>(constraint_ptr->operand()) ==
                BoardFunctions::NODE_ID) {
          send_node_id = true;
          skip = true;
        }
      }
      if (!skip) {
        auto current = sub.filter.serialize();
        if (max_size > 0 &&
            current.size() + serialized_sub.back().size() > max_size) {
          serialized_sub.emplace_back();
        }

        if (!serialized_sub.back().empty()) {
          serialized_sub.back() += "&" + std::move(current);
        } else {
          serialized_sub.back() = std::move(current);
        }
      }
    }
  }

  if (send_node_id) {
    if (!serialized_sub.back().empty()) {
      serialized_sub.back() +=
          "&" +
          Filter{Constraint{"node_id", BoardFunctions::NODE_ID}}.serialize();
    } else {
      serialized_sub.back() =
          Filter{Constraint{"node_id", BoardFunctions::NODE_ID}}.serialize();
    }
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
  auto [res_it, _inserted] = peer_node_ids.try_emplace(
      AString(subscriber_node_id), false, next_node_id++);
  uint32_t internal_node_id = res_it->second.second;

  if (auto it = std::find_if(
          subscriptions.begin(), subscriptions.end(),
          [&](const auto& sub_pair) { return sub_pair.second.filter == f; });
      it != subscriptions.end()) {
    Subscription<Allocator>& sub = it->second;
    bool updated = sub.add_receiver(r, internal_node_id);
    if (updated) {
      auto nodes = sub.nodes();
      if (nodes.size() == 2) {
        uint32_t other_sub = 0;
        for (const auto& n : nodes) {
          if (n != internal_node_id) {
            other_sub = n;
          }
        }

        auto exclude_node_id =
            std::find_if(peer_node_ids.begin(), peer_node_ids.end(),
                         [other_sub](const auto& node_it) {
                           return node_it.second.second == other_sub;
                         })
                ->first;
        publish_subscription_added(f, "", exclude_node_id);
      }
    }
    return sub.subscription_id;
  } else {  // new subscription
    uint32_t id = next_sub_id++;
    auto [sub_it, inserted] = subscriptions.try_emplace(
        id, id, r, internal_node_id);  // Filters not set yet;

    std::vector<std::shared_ptr<const Constraint>,
                Allocator<std::shared_ptr<const Constraint>>>
        required{make_allocator<std::shared_ptr<const Constraint>>()};
    std::vector<std::shared_ptr<const Constraint>,
                Allocator<std::shared_ptr<const Constraint>>>
        optional{make_allocator<std::shared_ptr<const Constraint>>()};

    // Index maintenance

    for (auto constraint : f.required) {
      auto [constraint_it, _inserted] = constraints.try_emplace(
          AString(constraint.attribute(), make_allocator<AString>()));
      auto cp = constraint_it->second.insert(std::move(constraint),
                                             &(sub_it->second), false);
      required.push_back(cp);
    }
    for (auto constraint : f.optional) {
      auto [constraint_it, _inserted] = constraints.try_emplace(
          AString(constraint.attribute(), make_allocator<AString>()));
      auto cp = constraint_it->second.insert(std::move(constraint),
                                             &(sub_it->second), true);
      optional.push_back(cp);
    }

    // Now the subscription is usable
    sub_it->second.set_constraints(std::move(required), std::move(optional));

    if (inserted && sub_it->second.count_required() == 0) {
      no_requirements.insert(&sub_it->second);
    }

    publish_subscription_added(
        f, AString(subscriber_node_id, make_allocator<AString>()), "");
    return id;
  }
}

uint32_t Router::remove_subscription(uint32_t sub_id, Receiver* r,
                                     std::string node_id) {
  size_t remaining = 0;
  std::unique_lock lck(mtx);

  uint32_t internal_node_id = 0;
  auto res_it = peer_node_ids.find(node_id);
  if (res_it != peer_node_ids.end()) {
    internal_node_id = res_it->second.second;
  }

  if (auto sub_it = subscriptions.find(sub_id); sub_it != subscriptions.end()) {
    auto& sub = sub_it->second;

    auto [empty_sub, rem] = sub.remove_receiver(r, internal_node_id);
    remaining = rem;

    if (empty_sub) {
      // Index maintenance

      if (sub.count_required() == 0) {
        no_requirements.erase(&sub);
      }

      for (auto constraint_ptr : sub.filter.required) {
        if (auto constraint_it = constraints.find(constraint_ptr->attribute());
            constraint_it != constraints.end()) {
          if (constraint_it->second.remove(*constraint_ptr, &sub)) {
            constraints.erase(constraint_it);
          }
        }
      }

      for (auto constraint_ptr : sub.filter.optional) {
        if (auto constraint_it = constraints.find(constraint_ptr->attribute());
            constraint_it != constraints.end()) {
          if (constraint_it->second.remove(*constraint_ptr, &sub)) {
            constraints.erase(constraint_it);
          }
        }
      }

      auto filter = std::move(sub.filter);

      subscriptions.erase(sub.subscription_id);

      publish_subscription_removed(filter, "", "");
    } else {
      auto n = sub.nodes();
      if (n.size() == 1 && *n.begin() != 1) {
        publish_subscription_removed(
            sub.filter, "",
            std::find_if(peer_node_ids.begin(), peer_node_ids.end(),
                         [&n](const auto& node_it) {
                           return node_it.second.second == *n.begin();
                         })
                ->first);
      }
    }
  }
  return remaining;
}

void Router::publish_subscription_added(const Filter& filter, AString exclude,
                                        AString include) {
  std::string serialized = filter.serialize();

  // Sub optimization
  for (const auto& constraint : filter.required) {
    if (constraint.attribute() == "publisher_node_id") {
      if (std::holds_alternative<std::string_view>(constraint.operand()) &&
          std::get<std::string_view>(constraint.operand()) ==
              BoardFunctions::NODE_ID) {
        include = "local";
      }
    }
    // Overapproximate node_id
    if (constraint.attribute() == "node_id" &&
        std::get<std::string_view>(constraint.operand()) ==
            BoardFunctions::NODE_ID) {
      for (auto& [node_id_string, node_id_info] : peer_node_ids) {
        if (node_id_string != exclude && !node_id_info.first) {
          Publication update{BoardFunctions::NODE_ID, "router", "1"};
          update.set_attr("type", "subscriptions_added");
          update.set_attr("serialized_subscriptions",
                          Filter{Constraint{"node_id", BoardFunctions::NODE_ID}}
                              .serialize());
          update.set_attr("include_node_id", node_id_string);
          publish_internal(std::move(update));
          node_id_info.first = true;
        }
      }
      return;
    }
  }

  Publication update{BoardFunctions::NODE_ID, "router", "1"};
  update.set_attr("type", "subscriptions_added");
  update.set_attr("serialized_subscriptions", std::move(serialized));
  if (exclude.length() != 0) {
    update.set_attr("exclude_node_id", exclude);
  }
  if (include.length() != 0) {
    update.set_attr("include_node_id", include);
  }
  publish_internal(std::move(update));
}

void Router::publish_subscription_removed(const InternalFilter& filter,
                                          AString exclude, AString include) {
  // Sub optimization
  for (auto constraint_ptr : filter.required) {
    if (constraint_ptr->attribute() == "publisher_node_id") {
      if (std::holds_alternative<std::string_view>(constraint_ptr->operand()) &&
          std::get<std::string_view>(constraint_ptr->operand()) ==
              BoardFunctions::NODE_ID) {
        include = "local";
      }
    }
    // TODO(raphaelhetzel) Keep track of the published node ids and remove them
    // as required
    if (constraint_ptr->attribute() == "node_id" &&
        std::get<std::string_view>(constraint_ptr->operand()) ==
            BoardFunctions::NODE_ID) {
      return;
    }
  }

  Publication update{BoardFunctions::NODE_ID, "router", "1"};
  update.set_attr("type", "subscriptions_removed");
  update.set_attr("serialized_subscriptions", filter.serialize());
  if (exclude.length() != 0) {
    update.set_attr("exclude_node_id", exclude);
  }
  if (include.length() != 0) {
    update.set_attr("include_node_id", include);
  }
  publish_internal(std::move(update));
}

ReceiverHandle Router::new_subscriber() { return ReceiverHandle{this}; }
}  // namespace uActor::PubSub
