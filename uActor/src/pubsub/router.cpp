#include "pubsub/router.hpp"

#ifdef ESP_IDF
#include <esp_attr.h>
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
#if CONFIG_UACTOR_OPTIMIZATIONS_IRAM
IRAM_ATTR
#endif
void Router::publish(Publication&& publication) {
  std::shared_lock lock(mtx);
  publish_internal(std::move(publication));
}

#if CONFIG_UACTOR_OPTIMIZATIONS_IRAM
IRAM_ATTR
#endif
void Router::publish_internal(Publication&& publication) {
  Counter<Allocator> c;

  uint8_t current_priority = 0;
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
      } else if (std::holds_alternative<std::shared_ptr<Publication::Map>>(
                     value)) {
        constraint_it->second.check(
            std::get<std::shared_ptr<Publication::Map>>(value).get(), &c);
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
        if (subscription_ptr->arguments.priority > current_priority) {
          current_priority = subscription_ptr->arguments.priority;
          current_receivers.clear();
        } else if (subscription_ptr->arguments.priority < current_priority) {
          continue;
        }
        for (auto& receiver : subscription_ptr->receivers) {
          current_receivers.emplace_back(receiver.first,
                                         subscription_ptr->subscription_id);
        }
      }
    }
  }

  // Check remaining subscriptions that might still match
  for (const auto& sub : no_requirements) {
    if (c.find(sub) == c.end()) {
      if (sub->filter.check_optionals(publication)) {
        if (sub->arguments.priority > current_priority) {
          current_priority = sub->arguments.priority;
          current_receivers.clear();
        } else if (sub->arguments.priority < current_priority) {
          continue;
        }
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

std::vector<std::shared_ptr<PubSub::Publication::Map>>
Router::subscription_lists_for(
    std::string_view node_id,
    std::function<bool(Filter*, const SubscriptionArguments&)> processing_chain,
    size_t num_subs) {
  std::shared_lock lock(mtx);

  std::vector<std::shared_ptr<PubSub::Publication::Map>> subscription_lists{
      std::make_shared<PubSub::Publication::Map>()};

  auto [peer_node_it, _inserted] =
      peer_node_ids.try_emplace(AString(node_id), false, next_node_id++);
  if (peer_node_it == peer_node_ids.end()) {
    return subscription_lists;
  }
  uint32_t internal_node_id = peer_node_it->second.second;

  for (auto& sub_pair : subscriptions) {
    auto& sub = sub_pair.second;

    // Only subscriptions that could be fulfilled
    auto n = sub.nodes();
    if (n.size() >= 2 || n.find(internal_node_id) == n.end()) {
      Filter filter = sub.filter.to_filter();

      if (!processing_chain(&filter, sub.arguments)) {
        std::shared_ptr<PubSub::Publication::Map> current_sub{
            std::make_shared<Publication::Map>()};
        current_sub->set_attr("subscription", filter.to_publication_map());
        current_sub->set_attr("subscription_arguments", sub.arguments.to_map());
        current_sub->set_attr(
            "subscriber",
            ActorIdentifier(BoardFunctions::NODE_ID, "replay", "1")
                .to_publication_map());

        if (num_subs > 0 && subscription_lists.back()->size() >= num_subs) {
          subscription_lists.emplace_back(
              std::make_shared<PubSub::Publication::Map>());
        }
        subscription_lists.back()->set_attr(
            std::to_string(subscription_lists.back()->size()),
            std::move(current_sub));
      }
    }
  }

  return subscription_lists;
}

std::vector<std::reference_wrapper<const Router::ASubscription>>
Router::find_subscriptions(std::function<bool(const Filter&)> filter) {
  std::map<std::string, std::vector<Constraint>> example_constraints;

  std::vector<std::reference_wrapper<const Router::ASubscription>> result;

  for (const auto& [sub_id, sub] : subscriptions) {
    if (filter(sub.filter.to_filter())) {
      result.push_back(sub);
    }
  }
  return result;
}

uint32_t Router::find_sub_id(const Filter& filter,
                             const SubscriptionArguments& arguments) {
  std::shared_lock lck(mtx);
  if (auto it = std::find_if(subscriptions.begin(), subscriptions.end(),
                             [&](const auto& sub) {
                               return sub.second.filter == filter &&
                                      sub.second.arguments == arguments;
                             });
      it != subscriptions.end()) {
    return it->first;
  }
  return 0;
}

uint32_t Router::number_of_subscriptions() { return subscriptions.size(); }

uint32_t Router::add_subscription(
    Filter&& f, Receiver* r,
    const ActorIdentifier& subscriber,  // True origin
    const std::string& source_peer,     // The node_id this came from
    SubscriptionArguments args) {
  std::unique_lock lck(mtx);

  if (is_meta_subscription(f)) {
    handle_meta_subscription(f, subscriber);
  }

  if (args.fetch_policy == FetchPolicy::FETCH) {
    AString exclude = AString(source_peer);
    if (source_peer == BoardFunctions::NODE_ID) {
      exclude = "";
    }
    publish_subscription_added(f, args, subscriber, std::move(exclude), "");
    return 1;
  }

  auto [res_it, _inserted] =
      peer_node_ids.try_emplace(AString(source_peer), false, next_node_id++);
  uint32_t internal_node_id = res_it->second.second;

  if (auto it = std::find_if(subscriptions.begin(), subscriptions.end(),
                             [&](const auto& sub_pair) {
                               return sub_pair.second.filter == f &&
                                      sub_pair.second.arguments == args;
                             });
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

        auto include_node_id =
            std::find_if(peer_node_ids.begin(), peer_node_ids.end(),
                         [other_sub](const auto& node_it) {
                           return node_it.second.second == other_sub;
                         })
                ->first;
        publish_subscription_added(f, args, subscriber, "", include_node_id);
      } else if (args.fetch_policy == FetchPolicy::FETCH_FUTURE) {
        AString exclude = AString(source_peer);
        if (source_peer == BoardFunctions::NODE_ID) {
          exclude = "";
        }
        publish_subscription_added(f, args, subscriber, exclude, "");
      } else if (source_peer == BoardFunctions::NODE_ID) {
        publish_subscription_added(f, args, subscriber, "",
                                   BoardFunctions::NODE_ID);
      }
    }
    return sub.subscription_id;
  } else {  // new subscription
    uint32_t id = next_sub_id++;
    auto [sub_it, inserted] = subscriptions.try_emplace(
        id, id, r, internal_node_id, args);  // Filters not set yet;

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

    AString exclude = AString(source_peer);
    if (source_peer == BoardFunctions::NODE_ID) {
      exclude = "";
    }
    publish_subscription_added(f, args, subscriber, exclude, "");
    return id;
  }
}

bool Router::is_meta_subscription(const Filter& filter) {
  for (auto& constraint : filter.required_constraints()) {
    if (constraint.attribute() == "type" &&
        std::holds_alternative<std::string_view>(constraint.operand()) &&
        constraint.predicate() == ConstraintPredicates::EQ &&
        std::get<std::string_view>(constraint.operand()) ==
            "local_subscription_added") {
      return true;
    }
  }
  return false;
}

void Router::handle_meta_subscription(const Filter& filter,
                                      const ActorIdentifier& subscriber) {
  auto accept_all = [](const Filter& filter) { return true; };

  for (auto sub : find_subscriptions(accept_all)) {
    auto pub = Publication(BoardFunctions::NODE_ID, "router", "1");
    pub.set_attr("node_id", subscriber.node_id);
    pub.set_attr("actor_type", subscriber.actor_type);
    pub.set_attr("instance_id", subscriber.instance_id);
    pub.set_attr("type", "local_subscription_exists");
    pub.set_attr("subscription",
                 std::move(sub.get().filter.to_filter().to_publication_map()));
    publish_internal(std::move(pub));
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
      auto arguments = std::move(sub.arguments);
      subscriptions.erase(sub.subscription_id);

      publish_subscription_removed(filter, arguments, "", "");
    } else {
      auto n = sub.nodes();
      if (n.size() == 1 && *n.begin() != 1) {
        publish_subscription_removed(
            sub.filter, sub.arguments, "",
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

void Router::publish_subscription_added(const Filter& filter,
                                        const SubscriptionArguments& arguments,
                                        const ActorIdentifier& actor_identifier,
                                        AString exclude, AString include) {
  Publication update{actor_identifier.node_id, actor_identifier.actor_type,
                     actor_identifier.instance_id};
  update.set_attr("type", "local_subscription_added");
  update.set_attr("subscriber", actor_identifier.to_publication_map());
  update.set_attr("subscription_arguments", arguments.to_map());
  update.set_attr("subscription", filter.to_publication_map());
  if (exclude.length() != 0) {
    update.set_attr("exclude_node_id", exclude);
  }
  if (include.length() != 0) {
    update.set_attr("include_node_id", include);
  }
  publish_internal(std::move(update));
}

void Router::publish_subscription_removed(
    const InternalFilter& filter, const SubscriptionArguments& arguments,
    AString exclude, AString include) {
  Publication update{BoardFunctions::NODE_ID, "router", "1"};
  update.set_attr("type", "local_subscription_removed");
  update.set_attr("subscription", filter.to_filter().to_publication_map());
  update.set_attr("subscription_arguments", arguments.to_map());
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
