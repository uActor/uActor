#include "remote/remote_connection.hpp"

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif
#if CONFIG_BENCHMARK_ENABLED
#include <support/testbed.h>
#endif

#include <algorithm>
#include <cassert>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "board_functions.hpp"
#include "controllers/telemetry_data.hpp"
#include "pubsub/router.hpp"
#include "remote/forwarder_api.hpp"
#include "remote/sequence_number_forwarding_strategy.hpp"
#include "support/logger.hpp"

namespace uActor::Remote {

RemoteConnection::RemoteConnection(uint32_t local_id, int32_t socket_id,
                                   std::string remote_addr,
                                   uint16_t remote_port,
                                   ConnectionRole connection_role,
                                   ForwarderSubscriptionAPI* handle)
    : local_id(local_id),
      sock(socket_id),
      partner_ip(remote_addr),
      partner_port(remote_port),
      connection_role(connection_role),
      handle(handle),
      forwarding_strategy(
          std::make_unique<SequenceNumberForwardingStrategy>()) {
  update_sub_id = handle->add_subscription(
      local_id,
      PubSub::Filter{
          PubSub::Constraint{"type", "subscription_update"},
          PubSub::Constraint{"update_receiver_id", std::to_string(local_id)},
          PubSub::Constraint{"publisher_node_id",
                             std::string(BoardFunctions::NODE_ID)}},
      "local");
}

RemoteConnection::~RemoteConnection() {
  handle->remove_subscription(local_id, update_sub_id, "");
  if (add_sub_id != 0) {
    handle->remove_subscription(local_id, add_sub_id, "");
  }
  if (remove_sub_id != 0) {
    handle->remove_subscription(local_id, remove_sub_id, "");
  }
  for (const auto& subscription_id : subscription_ids) {
    handle->remove_subscription(local_id, subscription_id, std::string(""));
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

void RemoteConnection::handle_subscription_update_notification(
    const PubSub::Publication& /*update_message*/) {}

void RemoteConnection::send_routing_info() {
  PubSub::Publication p{BoardFunctions::NODE_ID, "remote_connection",
                        std::to_string(local_id)};
  p.set_attr("type", "subscription_update");
  p.set_attr("update_receiver_id", std::to_string(local_id));
  p.set_attr("subscription_node_id", std::string(BoardFunctions::NODE_ID));
  if (!partner_node_id.empty()) {
    p.set_attr(
        "serialized_subscriptions",
        PubSub::Router::get_instance().subscriptions_for(partner_node_id));
  }
  PubSub::Router::get_instance().publish(std::move(p));
}

void RemoteConnection::process_data(uint32_t len, char* data) {
  uint32_t bytes_remaining = len;
  while (bytes_remaining > 0) {
    if (state == empty) {
      if (bytes_remaining >= 4) {
        publicaton_full_size =
            ntohl(*reinterpret_cast<uint32_t*>(data + (len - bytes_remaining)));
        publicaton_remaining_bytes = publicaton_full_size;
        bytes_remaining -= 4;
        state = waiting_for_data;
      } else {
        std::memcpy(size_buffer, data + (len - bytes_remaining),
                    bytes_remaining);
        size_field_remaining_bytes = 4 - bytes_remaining;
        bytes_remaining = 0;
        state = waiting_for_size;
      }
    } else if (state == waiting_for_size) {
      if (bytes_remaining > size_field_remaining_bytes) {
        std::memcpy(size_buffer + (4 - size_field_remaining_bytes),
                    data + (len - bytes_remaining), size_field_remaining_bytes);
        publicaton_full_size = ntohl(*reinterpret_cast<uint32_t*>(size_buffer));
        publicaton_remaining_bytes = publicaton_full_size;
        bytes_remaining -= size_field_remaining_bytes;
        size_field_remaining_bytes = 0;
        state = waiting_for_data;
      } else {
        std::memcpy(size_buffer + (4 - size_field_remaining_bytes),
                    data + (len - bytes_remaining), bytes_remaining);
        size_field_remaining_bytes =
            size_field_remaining_bytes - bytes_remaining;
        bytes_remaining = 0;
      }
    } else if (state == waiting_for_data) {
      uint32_t to_move = std::min(publicaton_remaining_bytes, bytes_remaining);
      publication_buffer.write(data + (len - bytes_remaining), to_move);
      bytes_remaining -= to_move;
      publicaton_remaining_bytes -= to_move;
      if (publicaton_remaining_bytes == 0) {
#if CONFIG_BENCHMARK_BREAKDOWN
        timeval tv;
        gettimeofday(&tv, NULL);
#endif
        std::optional<PubSub::Publication> p;
        if (publicaton_full_size > 0) {
          p = publication_buffer.build();
        } else {
          // printf("null message\n");
        }
        if (p) {
          process_publication(std::move(*p));
        }
        publication_buffer = PubSub::PublicationFactory();
        state = empty;
        assert(publicaton_remaining_bytes == 0);
      }
    }
  }
}

void RemoteConnection::process_publication(PubSub::Publication&& p) {
  if (p.has_attr("publisher_node_id") &&
      p.get_str_attr("publisher_node_id") != BoardFunctions::NODE_ID) {
    auto publisher_node_id = std::string(*p.get_str_attr("publisher_node_id"));
    uActor::Support::Logger::trace("REMOTE-CONNECTION", "MESSAGE from",
                                   publisher_node_id.c_str());

    if (forwarding_strategy->should_accept(p)) {
      if (p.get_str_attr("type") == "subscription_update") {
        update_subscriptions(std::move(p));
#if CONFIG_UACTOR_ENABLE_TELEMETRY
        Controllers::TelemetryData::increase("sub_traffic_size",
                                             publicaton_full_size);
#endif
      } else if (p.get_str_attr("type") == "subscription_added") {
        add_subscription(std::move(p));
#if CONFIG_UACTOR_ENABLE_TELEMETRY
        Controllers::TelemetryData::increase("sub_traffic_size",
                                             publicaton_full_size);
#endif
      } else if (p.get_str_attr("type") == "subscription_removed") {
        remove_subscription(std::move(p));
#if CONFIG_UACTOR_ENABLE_TELEMETRY
        Controllers::TelemetryData::increase("sub_traffic_size",
                                             publicaton_full_size);
#endif
      } else {
#if CONFIG_UACTOR_ENABLE_TELEMETRY
        if (p.get_str_attr("type") == "deployment") {
          Controllers::TelemetryData::increase("deployment_traffic_size",
                                               publicaton_full_size);
        } else {
          Controllers::TelemetryData::increase("regular_traffic_size",
                                               publicaton_full_size);
        }
#endif
        forwarding_strategy->add_incomming_routing_fields(&p);
        PubSub::Router::get_instance().publish(std::move(p));
      }
#if CONFIG_BENCHMARK_ENABLED
      current_traffic.num_accepted_messages++;
      current_traffic.size_accepted_messages += publicaton_full_size;
#endif
    } else {
      Support::Logger::debug("REMOTE-CONNECTION", "RECEIVE",
                             "Message Dropped from %s, forwarded-by: %s",
                             publisher_node_id.c_str(),
                             partner_node_id.c_str());
#if CONFIG_BENCHMARK_ENABLED
      current_traffic.num_duplicate_messages++;
      current_traffic.size_duplicate_messages += publicaton_full_size;
#endif
    }
  }
}

void RemoteConnection::add_subscription(PubSub::Publication&& p) {
  if (p.has_attr("serialized_subscription")) {
    auto deserialized =
        PubSub::Filter::deserialize(*p.get_str_attr("serialized_subscription"));
    uint32_t sub_id = handle->add_subscription(
        local_id, PubSub::Filter(*deserialized), partner_node_id);
    subscription_ids.insert(sub_id);
  }
}

void RemoteConnection::remove_subscription(PubSub::Publication&& p) {
  if (p.has_attr("serialized_subscription")) {
    auto deserialized =
        PubSub::Filter::deserialize(*p.get_str_attr("serialized_subscription"));
    if (deserialized) {
      printf("CALLED UNSUB\n");
      uint32_t sub_id =
          PubSub::Router::get_instance().find_sub_id(std::move(*deserialized));
      handle->remove_subscription(local_id, sub_id, partner_node_id);
      subscription_ids.erase(sub_id);
    }
  }
}

void RemoteConnection::update_subscriptions(PubSub::Publication&& p) {
  std::string_view new_partner_id = *p.get_str_attr("subscription_node_id");

  if (partner_node_id.empty()) {
    PubSub::Publication update{BoardFunctions::NODE_ID, "remote_connection",
                               std::to_string(local_id)};
    update.set_attr("type", "peer_update");
    update.set_attr("update_type", "peer_added");
    update.set_attr("peer_node_id", new_partner_id);
    PubSub::Router::get_instance().publish(std::move(update));

    partner_node_id = std::string(new_partner_id);

    send_routing_info();

    add_sub_id = handle->add_subscription(
        local_id,
        PubSub::Filter{
            PubSub::Constraint{"type", "subscription_added"},
            PubSub::Constraint{"include_node_id", partner_node_id,
                               uActor::PubSub::ConstraintPredicates::EQ, true},
            PubSub::Constraint{"exclude_node_id", partner_node_id,
                               uActor::PubSub::ConstraintPredicates::NE, true},
            PubSub::Constraint{"publisher_node_id",
                               std::string(BoardFunctions::NODE_ID)}},
        "local");

    remove_sub_id = handle->add_subscription(
        local_id,
        PubSub::Filter{
            PubSub::Constraint{"type", "subscription_removed"},
            PubSub::Constraint{"include_node_id", partner_node_id,
                               uActor::PubSub::ConstraintPredicates::EQ, true},
            PubSub::Constraint{"exclude_node_id", partner_node_id,
                               uActor::PubSub::ConstraintPredicates::NE, true},
            PubSub::Constraint{"publisher_node_id",
                               std::string(BoardFunctions::NODE_ID)}},
        "local");

    forwarding_strategy->partner_node_id(partner_node_id);

    // Flooding
    // subscription_ids.push_back(handle->add_subscription(local_id,
    // PubSub::Filter{}, std::string_view(partner_node_id)));
  }

  if (p.has_attr("serialized_subscriptions")) {
    std::unordered_set<uint32_t> old_subscriptions =
        std::unordered_set<uint32_t>(subscription_ids.begin(),
                                     subscription_ids.end());
    for (auto serialized : Support::StringHelper::string_split(
             *p.get_str_attr("serialized_subscriptions"), "&")) {
      auto deserialized = PubSub::Filter::deserialize(serialized);
      if (deserialized) {
        uint32_t sub_id = handle->add_subscription(
            local_id, PubSub::Filter(*deserialized), partner_node_id);
        // for (const auto constraint : deserialized->required) {
        //   if (constraint.attribute() == "node_id" &&
        //       std::holds_alternative<std::string>(constraint.operand())) {
        //     uActor::Support::Logger::trace(
        //         "REMOTE", "SUB-ID", "%s - %d",
        //         std::get<std::string>(constraint.operand()).c_str(), sub_id);
        //   }
        // }
        if (old_subscriptions.erase(sub_id) == 0) {
          subscription_ids.insert(sub_id);
          uActor::Support::Logger::trace("REMOTE", "SUB-ID ADDED", "%d",
                                         sub_id);
        }
      }
    }
    for (auto subscription_id : old_subscriptions) {
      handle->remove_subscription(local_id, subscription_id, partner_node_id);
      subscription_ids.erase(subscription_id);
      uActor::Support::Logger::trace("REMOTE", "SUB-ID REMOVED", "%d",
                                     subscription_id);
    }
  }
}

#if CONFIG_BENCHMARK_ENABLED
ConnectionTraffic RemoteConnection::current_traffic;
#endif

}  // namespace uActor::Remote
