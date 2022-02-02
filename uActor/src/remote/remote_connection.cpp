#include "remote/remote_connection.hpp"

#ifdef ESP_IDF
#include <sdkconfig.h>
// CONFIG_UACTOR_IS_LEAF = 1
#endif
#if CONFIG_BENCHMARK_ENABLED
#include <support/testbed.h>
#endif

#include <algorithm>
#include <cassert>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "board_functions.hpp"
#include "controllers/telemetry_data.hpp"
#include "pubsub/actor_identifier.hpp"
#include "pubsub/router.hpp"
#include "pubsub/subscription_arguments.hpp"
#include "remote/forwarder_api.hpp"
#include "remote/sequence_number_forwarding_strategy.hpp"
#include "remote/subscription_processors/cluster_aggregator.hpp"
#include "remote/subscription_processors/cluster_barrier.hpp"
#include "remote/subscription_processors/leaf_node_id_gateway.hpp"
#include "remote/subscription_processors/node_id_aggregator.hpp"
#include "remote/subscription_processors/node_local_filter_drop.hpp"
#include "remote/subscription_processors/optional_constraint_drop.hpp"
#include "remote/subscription_processors/scope_cluster.hpp"
#include "remote/subscription_processors/scope_local.hpp"
#include "support/logger.hpp"

namespace uActor::Remote {

RemoteConnection::RemoteConnection(uint32_t local_id,
                                   ForwarderSubscriptionAPI* handle)
    : local_id(local_id),
      handle(handle),
      forwarding_strategy(
          std::make_unique<SequenceNumberForwardingStrategy>()) {
  update_sub_id = handle->add_local_subscription(
      local_id,
      PubSub::Filter{
          PubSub::Constraint{"type", "subscription_update"},
          PubSub::Constraint{"update_receiver_id", std::to_string(local_id)},
          PubSub::Constraint{"publisher_node_id",
                             std::string(BoardFunctions::NODE_ID)}});
}

RemoteConnection::~RemoteConnection() {
  handle->remove_local_subscription(local_id, update_sub_id);
  if (remote_subs_added_id != 0) {
    handle->remove_remote_subscription(local_id, remote_subs_added_id, "");
  }
  if (remote_subs_removed_id != 0) {
    handle->remove_remote_subscription(local_id, remote_subs_removed_id, "");
  }
  if (local_sub_added_id != 0) {
    handle->remove_remote_subscription(local_id, local_sub_added_id, "");
  }
  if (local_sub_removed_id != 0) {
    handle->remove_remote_subscription(local_id, local_sub_removed_id, "");
  }
  for (const auto& subscription_id : subscription_ids) {
    handle->remove_remote_subscription(local_id, subscription_id,
                                       std::string(""));
  }
  subscription_ids.clear();
  if (!partner_node_id.empty()) {
    PubSub::Publication update{BoardFunctions::NODE_ID, "remote_connection",
                               std::to_string(local_id)};
    update.set_attr("type", "peer_update");
    update.set_attr("update_type", "peer_removed");
    update.set_attr("peer_node_id", partner_node_id);
    PubSub::Router::get_instance().publish(std::move(update));
  }
}

void RemoteConnection::send_routing_info() {
  PubSub::Publication p{BoardFunctions::NODE_ID, "remote_connection",
                        std::to_string(local_id)};
  p.set_attr("type", "subscription_update");

  if (clusters.size() > 0) {
    std::string cluster_keys = "";
    for (const auto& cluster : clusters) {
      std::string current_cluster_keys = "";
      for (const auto& label : cluster) {
        if (current_cluster_keys.size() > 0) {
          current_cluster_keys += std::string(",") + label;
        } else {
          current_cluster_keys += label;
        }
        p.set_attr(label, local_location_labels[label]);
      }
      if (cluster_keys.size() > 0) {
        cluster_keys += std::string(";") + current_cluster_keys;
      } else {
        cluster_keys += current_cluster_keys;
      }
    }
    p.set_attr("cluster_keys", std::move(cluster_keys));
  }

  p.set_attr("update_receiver_id", std::to_string(local_id));
  p.set_attr("subscription_node_id", std::string(BoardFunctions::NODE_ID));
  PubSub::Router::get_instance().publish(std::move(p));
}

void RemoteConnection::process_remote_publication(PubSub::Publication&& p,
                                                  size_t encoded_length) {
  if (p.has_attr("publisher_node_id") &&
      p.get_str_attr("publisher_node_id") != BoardFunctions::NODE_ID) {
    auto publisher_node_id = std::string(*p.get_str_attr("publisher_node_id"));
    uActor::Support::Logger::trace("REMOTE-CONNECTION", "MESSAGE from: %s",
                                   publisher_node_id.c_str());

    if (forwarding_strategy->should_accept(p)) {
      if (p.get_str_attr("type") == "subscription_update") {
        handle_remote_hello(std::move(p));
#if CONFIG_UACTOR_ENABLE_TELEMETRY
        Controllers::TelemetryData::increase("sub_traffic_size",
                                             encoded_length);
#endif
      } else if (p.get_str_attr("type") == "subscriptions_added") {
        handle_remote_subscriptions_added(std::move(p));
#if CONFIG_UACTOR_ENABLE_TELEMETRY
        Controllers::TelemetryData::increase("sub_traffic_size",
                                             encoded_length);
#endif
      } else if (p.get_str_attr("type") == "subscriptions_removed") {
        handle_remote_subscriptions_removed(std::move(p));
#if CONFIG_UACTOR_ENABLE_TELEMETRY
        Controllers::TelemetryData::increase("sub_traffic_size",
                                             encoded_length);
#endif
      } else {
#if CONFIG_UACTOR_ENABLE_TELEMETRY
        if (p.get_str_attr("type") == "deployment" ||
            p.get_str_attr("type") == "actor_code") {
          Controllers::TelemetryData::increase("deployment_traffic_size",
                                               encoded_length);
          Controllers::TelemetryData::increase("deployment_traffic_number", 1);
        } else {
          Controllers::TelemetryData::increase("regular_traffic_size",
                                               encoded_length);
          Controllers::TelemetryData::increase("regular_traffic_number", 1);
        }
#endif

        {
          for (const auto& cluster : clusters) {
            bool same_cluster = true;
            for (const auto& key : cluster) {
              if (local_location_labels[key] != partner_location_labels[key]) {
                same_cluster = false;
                break;
              }
            }
            if (!same_cluster) {
              std::string reply_path_addon = "";
              for (const auto& key : cluster) {
                p.set_attr(std::string("publisher_") + key,
                           local_location_labels[key]);
                reply_path_addon = reply_path_addon + key + ",";
              }
              if (p.has_attr("reply_path")) {
                p.set_attr("reply_path",
                           reply_path_addon +
                               std::string(*p.get_str_attr("reply_path")));
              } else {
                p.set_attr("reply_path",
                           reply_path_addon + "node_id,actor_type,instance_id");
              }
            }
          }
        }

        forwarding_strategy->add_incomming_routing_fields(&p);
        PubSub::Router::get_instance().publish(std::move(p));
      }
    } else {
      Support::Logger::debug(
          "REMOTE-CONNECTION"
          "Received message dropped. From: %s, Forwarded-by: %s",
          publisher_node_id.c_str(), partner_node_id.c_str());
#if CONFIG_UACTOR_ENABLE_TELEMETRY
      Controllers::TelemetryData::increase("dropped_traffic_size",
                                           encoded_length);
      Controllers::TelemetryData::increase("dropped_traffic_number", 1);
#endif
    }
  }
}

std::map<std::string, std::string> RemoteConnection::local_location_labels;
std::list<std::vector<std::string>> RemoteConnection::clusters;
std::string RemoteConnection::cluster;  //  NOLINT

void RemoteConnection::handle_remote_subscriptions_added(
    const PubSub::Publication& p) {
  if (auto subscription_list = p.get_nested_component("subscriptions");
      subscription_list) {
    for (size_t i = 0; i < (*subscription_list)->size(); i++) {
      auto current_sub_map =
          (*subscription_list)->get_nested_component(std::to_string(i));

      if (!current_sub_map) {
        continue;
      }

      auto subscription_raw =
          (*current_sub_map)->get_nested_component("subscription");
      auto arguments_raw =
          (*current_sub_map)->get_nested_component("subscription_arguments");
      auto subscriber_raw =
          (*current_sub_map)->get_nested_component("subscriber");

      if (subscription_raw && arguments_raw && subscriber_raw) {
        auto deserialized =
            PubSub::Filter::from_publication_map(**subscription_raw);
        if (!deserialized) {
          Support::Logger::warning(
              "REMOTE-CONNECTION",
              "Malformed subscription added: Fiter error.");
          continue;
        }

        auto subscriber =
            PubSub::ActorIdentifier::from_publication_map(**subscriber_raw);

        if (!subscriber) {
          Support::Logger::warning(
              "REMOTE-CONNECTION",
              "Malformed subscription added: Subscriber error.");
          continue;
        }

        auto arguments =
            PubSub::SubscriptionArguments::from_map(**arguments_raw);

        auto current_subscription_rule = *deserialized;
        bool skip = false;

        for (auto& ingress_processor : ingress_subscription_processors) {
          if (ingress_processor->process_added(&current_subscription_rule,
                                               arguments)) {
            skip = true;
          }
        }
        if (skip) {
          continue;
        }
        uint32_t sub_id = handle->add_remote_subscription(
            local_id, std::move(current_subscription_rule), partner_node_id,
            *subscriber, arguments);
        subscription_ids.insert(sub_id);
      } else {
        Support::Logger::warning("REMOTE-CONNECTION",
                                 "Malformed subscription added.");
      }
    }
  }
}

void RemoteConnection::handle_remote_subscriptions_removed(
    const PubSub::Publication& p) {
  if (auto subscription_list_map = p.get_nested_component("subscriptions");
      subscription_list_map) {
    for (size_t i = 0; i < (*subscription_list_map)->size(); i++) {
      auto current_subscription_map =
          (*subscription_list_map)->get_nested_component(std::to_string(i));

      if (!current_subscription_map) {
        Support::Logger::warning("REMOTE-CONNECTION",
                                 "Malformed subscription removed.");
        continue;
      }

      auto filter_raw =
          (*current_subscription_map)->get_nested_component("subscription");
      auto arguments_raw = (*current_subscription_map)
                               ->get_nested_component("subscription_arguments");
      // subscriber information is not transfered for removed subscriptions

      if (filter_raw && arguments_raw) {
        auto deserialized = PubSub::Filter::from_publication_map(**filter_raw);
        auto arguments =
            PubSub::SubscriptionArguments::from_map(**arguments_raw);

        if (!deserialized) {
          Support::Logger::warning("REMOTE-CONNECTION",
                                   "Malformed subscription removed: Fiter.");
          continue;
        }

        auto current_subscription_rule = *deserialized;
        bool skip = false;

        for (auto& ingress_processor : ingress_subscription_processors) {
          if (ingress_processor->process_removed(&current_subscription_rule,
                                                 arguments)) {
            skip = true;
          }
        }
        if (skip) {
          continue;
        }
        uint32_t sub_id = PubSub::Router::get_instance().find_sub_id(
            std::move(current_subscription_rule), std::move(arguments));
        handle->remove_remote_subscription(local_id, sub_id, partner_node_id);
        subscription_ids.erase(sub_id);
      } else {
        Support::Logger::warning("REMOTE-CONNECTION",
                                 "Malformed subscription removed.");
        continue;
      }
    }
  }
}

void RemoteConnection::handle_local_subscription_removed(
    const PubSub::Publication& p) {
  if (!p.has_attr("subscription")) {
    Support::Logger::warning(
        "REMOTE-CONNECTION",
        "Filter missing from local subscription (removed).");
    return;
  }

  auto o_current_subscription_rule = PubSub::Filter::from_publication_map(
      **p.get_nested_component("subscription"));
  if (!o_current_subscription_rule) {
    Support::Logger::warning(
        "REMOTE-CONNECTION",
        "Malformed filter in local subscription message (removed).");
    return;
  }
  auto current_subscription_rule = *o_current_subscription_rule;

  PubSub::SubscriptionArguments arguments;
  auto raw_args = p.get_nested_component("subscription_arguments");
  if (raw_args) {
    arguments = PubSub::SubscriptionArguments::from_map(**raw_args);
  } else {
    Support::Logger::warning(
        "REMOTE-CONNECTION",
        "Arguments missing from local subscription (removed).");
    return;
  }

  // Edit rules

  for (auto& processor : egress_subscription_processors) {
    if (processor->process_removed(&current_subscription_rule, arguments)) {
      return;
    }
  }

  // Send final rule update
  PubSub::Publication rsr_message(BoardFunctions::NODE_ID, "remote_connection",
                                  std::to_string(local_id));
  rsr_message.set_attr("type", "subscriptions_removed");
  rsr_message.set_attr("remote_id", static_cast<int32_t>(local_id));

  auto sub = std::make_shared<PubSub::Publication::Map>();
  sub->set_attr("subscription", current_subscription_rule.to_publication_map());
  sub->set_attr("subscription_arguments", arguments.to_map());
  // The receiver currently does not differentiate between multiple and single
  // subscriptions, so this sends a list
  auto sub_list = std::make_shared<PubSub::Publication::Map>();
  sub_list->set_attr("0", std::move(sub));
  rsr_message.set_attr("subscriptions", std::move(sub_list));

  PubSub::Router::get_instance().publish(std::move(rsr_message));
}

void RemoteConnection::handle_local_subscription_added(
    const PubSub::Publication& p) {
  if (!p.has_attr("subscription")) {
    Support::Logger::warning("REMOTE-CONNECTION",
                             "Filter missing from local subscription (added).");
    return;
  }

  auto o_current_subscription_rule = PubSub::Filter::from_publication_map(
      **p.get_nested_component("subscription"));
  if (!o_current_subscription_rule) {
    return;
  }
  auto current_subscription_rule = *o_current_subscription_rule;

  PubSub::SubscriptionArguments arguments;
  auto raw_args = p.get_nested_component("subscription_arguments");
  if (raw_args) {
    arguments = PubSub::SubscriptionArguments::from_map(**raw_args);
  } else {
    return Support::Logger::warning(
        "REMOTE-CONNECTION",
        "Arguments missing from local subscription (added).");
  }

  auto raw_subscriber = p.get_nested_component("subscriber");
  if (!raw_subscriber) {
    Support::Logger::warning(
        "REMOTE-CONNECTION",
        "Subscriber missing from local subscription (added).");
    return;
  }

  auto opt_subscriber =
      PubSub::ActorIdentifier::from_publication_map(**raw_subscriber);

  if (!opt_subscriber) {
    Support::Logger::warning(
        "REMOTE-CONNECTION",
        "Subscriber components missing from local subscription (added).");
    return;
  }

  // Edit rule
  for (auto& processor : egress_subscription_processors) {
    if (processor->process_added(&current_subscription_rule, arguments)) {
      return;
    }
  }

  // Send final rule update
  PubSub::Publication rsr_message(BoardFunctions::NODE_ID, "remote_connection",
                                  std::to_string(local_id));
  rsr_message.set_attr("type", "subscriptions_added");
  rsr_message.set_attr("remote_id", static_cast<int32_t>(local_id));

  // The receiver currently does not differentiate between multiple and single
  // subscriptions, so this sends a list
  auto sub = std::make_shared<PubSub::Publication::Map>();
  sub->set_attr("subscription", current_subscription_rule.to_publication_map());
  sub->set_attr("subscription_arguments", arguments.to_map());
  sub->set_attr("subscriber", opt_subscriber->to_publication_map());

  auto sub_list = std::make_shared<PubSub::Publication::Map>();
  sub_list->set_attr("0", std::move(sub));
  rsr_message.set_attr("subscriptions", std::move(sub_list));

  PubSub::Router::get_instance().publish(std::move(rsr_message));
}

void RemoteConnection::handle_remote_hello(PubSub::Publication&& p) {
  std::string_view new_partner_id = *p.get_str_attr("subscription_node_id");

  if (partner_node_id.empty()) {
    PubSub::Publication update{BoardFunctions::NODE_ID, "remote_connection",
                               std::to_string(local_id)};
    update.set_attr("type", "peer_update");
    update.set_attr("update_type", "peer_added");
    update.set_attr("peer_node_id", new_partner_id);
    PubSub::Router::get_instance().publish(std::move(update));

    partner_node_id = std::string(new_partner_id);

    for (const auto& cluster_keys : clusters) {
      for (const auto& label : cluster_keys) {
        if (p.has_attr(label)) {
          partner_location_labels[label] = *p.get_str_attr(label);
        }
      }
    }

    auto raw_peer_cluster = p.get_str_attr("cluster");
    if (raw_peer_cluster) {
      peer_cluster = *raw_peer_cluster;
    }

    send_routing_info();

    local_sub_added_id = handle->add_local_subscription(
        local_id,
        PubSub::Filter{
            PubSub::Constraint{"type", "local_subscription_added"},
            PubSub::Constraint{"include_node_id", partner_node_id,
                               uActor::PubSub::ConstraintPredicates::EQ, true},
            PubSub::Constraint{"exclude_node_id", partner_node_id,
                               uActor::PubSub::ConstraintPredicates::NE, true},
            PubSub::Constraint{"publisher_node_id",
                               std::string(BoardFunctions::NODE_ID)}});

    local_sub_removed_id = handle->add_local_subscription(
        local_id,
        PubSub::Filter{
            PubSub::Constraint{"type", "local_subscription_removed"},
            PubSub::Constraint{"include_node_id", partner_node_id,
                               uActor::PubSub::ConstraintPredicates::EQ, true},
            PubSub::Constraint{"exclude_node_id", partner_node_id,
                               uActor::PubSub::ConstraintPredicates::NE, true},
            PubSub::Constraint{"publisher_node_id",
                               std::string(BoardFunctions::NODE_ID)}});

    // TODO(raphaelhetzel) this is a local subscription serving the remote node.
    remote_subs_added_id = handle->add_remote_subscription(
        local_id,
        PubSub::Filter{
            PubSub::Constraint{"type", "subscriptions_added"},
            PubSub::Constraint{"remote_id", static_cast<int32_t>(local_id)},
            PubSub::Constraint{"publisher_node_id",
                               std::string(BoardFunctions::NODE_ID)}},
        BoardFunctions::NODE_ID,
        PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "remote_connection",
                                "1"));

    // TODO(raphaelhetzel) this is a local subscription serving the remote node.
    remote_subs_removed_id = handle->add_remote_subscription(
        local_id,
        PubSub::Filter{
            PubSub::Constraint{"type", "subscriptions_removed"},
            PubSub::Constraint{"remote_id", static_cast<int32_t>(local_id)},
            PubSub::Constraint{"publisher_node_id",
                               std::string(BoardFunctions::NODE_ID)}},
        BoardFunctions::NODE_ID,
        PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "remote_connection",
                                "1"));

    forwarding_strategy->partner_node_id(partner_node_id);

    egress_subscription_processors.push_back(
        std::make_unique<NodeLocalFilterDrop>(BoardFunctions::NODE_ID,
                                              partner_node_id));
    egress_subscription_processors.push_back(std::make_unique<NodeIdAggregator>(
        BoardFunctions::NODE_ID, partner_node_id));
    for (const auto& cluster : clusters) {
      auto cl = ClusterLabels{};
      for (const auto& key : cluster) {
        cl.emplace(key, ClusterLabelValuePair(local_location_labels[key],
                                              partner_location_labels[key]));
      }
      egress_subscription_processors.push_back(
          std::make_unique<ClusterAggregator>(BoardFunctions::NODE_ID,
                                              partner_node_id, cl));
      egress_subscription_processors.push_back(std::make_unique<ClusterBarrier>(
          BoardFunctions::NODE_ID, partner_node_id, std::move(cl),
          KeyList{std::string("node_id")}));
    }

    egress_subscription_processors.push_back(
        std::make_unique<ScopeLocal>(BoardFunctions::NODE_ID, partner_node_id));
    egress_subscription_processors.push_back(std::make_unique<ScopeCluster>(
        BoardFunctions::NODE_ID, partner_node_id, cluster, peer_cluster));

    egress_subscription_processors.push_back(
        std::make_unique<OptionalConstraintDrop>(
            BoardFunctions::NODE_ID, partner_node_id,
            std::set<PubSub::Constraint>{
                {"node_id", BoardFunctions::NODE_ID,
                 PubSub::ConstraintPredicates::Predicate::EQ, true}}));

#ifdef CONFIG_UACTOR_IS_LEAF
    ingress_subscription_processors.push_back(
        std::make_unique<LeafNodeIdGateway>(BoardFunctions::NODE_ID,
                                            partner_node_id));
#endif

    for (const auto &chunk :
         PubSub::Router::get_instance().subscription_lists_for(
             partner_node_id,
             [&](PubSub::Filter*f,
                 const PubSub::SubscriptionArguments&arguments) {
               for (auto& processor : egress_subscription_processors) {
                 if (processor->process_added(f, arguments)) {
                   return true;
                 }
               }
               return false;
             },
             5)) {
      PubSub::Publication p{BoardFunctions::NODE_ID, "remote_connection",
                            std::to_string(local_id)};
      p.set_attr("type", "subscriptions_added");
      p.set_attr("remote_id", static_cast<int32_t>(local_id));
      p.set_attr("subscriptions", std::move(chunk));
      PubSub::Router::get_instance().publish(std::move(p));
    }

    // Flooding
    // subscription_ids.push_back(handle->add_subscription(local_id,
    // PubSub::Filter{}, std::string_view(partner_node_id)));
  }
}

}  // namespace uActor::Remote
