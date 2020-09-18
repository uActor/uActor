#include "remote/remote_connection.hpp"

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif
#if CONFIG_BENCHMARK_BREAKDOWN
#include <support/testbed.h>
#endif

#include <algorithm>
#include <cassert>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "board_functions.hpp"
#include "pubsub/router.hpp"
#include "remote/forwarder_api.hpp"
#include "support/logger.hpp"

namespace uActor::Remote {

std::unordered_map<std::string, Remote::SequenceInfo>
    RemoteConnection::sequence_infos;
std::atomic<uint32_t> RemoteConnection::sequence_number{1};
std::mutex RemoteConnection::mtx;

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
      handle(handle) {
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
    const PubSub::Publication& update_message) {
  send_routing_info();
}

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
        publication_buffer.resize(publicaton_full_size);
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
        publication_buffer.resize(publicaton_full_size);
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
      std::memcpy(publication_buffer.data() +
                      (publicaton_full_size - publicaton_remaining_bytes),
                  data + (len - bytes_remaining), to_move);
      bytes_remaining -= to_move;
      publicaton_remaining_bytes -= to_move;
      if (publicaton_remaining_bytes == 0) {
#if CONFIG_BENCHMARK_BREAKDOWN
        timeval tv;
        gettimeofday(&tv, NULL);
#endif
        std::optional<PubSub::Publication> p;
        if (publicaton_full_size > 0) {
          p = PubSub::Publication::from_msg_pack(std::string_view(
              publication_buffer.data(), publicaton_full_size));
        } else {
          // printf("null message\n");
        }
        if (p && p->has_attr("publisher_node_id") &&
            p->get_str_attr("publisher_node_id") != BoardFunctions::NODE_ID) {
          auto publisher_node_id = p->get_str_attr("publisher_node_id");
          uActor::Support::Logger::trace("REMOTE-CONNECTION", "MESSAGE",
                                         publisher_node_id->data());
          auto sequence_number = p->get_int_attr("_internal_sequence_number");
          auto epoch_number = p->get_int_attr("_internal_epoch");
          std::unique_lock lck(mtx);
          auto sequence_info_it =
              sequence_infos.find(std::string(*publisher_node_id));
          if (sequence_info_it == sequence_infos.end() ||
              (epoch_number && sequence_number &&
               sequence_info_it->second.is_older_than(*sequence_number,
                                                      *epoch_number))) {
            auto new_sequence_info = Remote::SequenceInfo(
                *sequence_number, *epoch_number, BoardFunctions::timestamp());
            if (sequence_info_it != sequence_infos.end()) {
              sequence_info_it->second = std::move(new_sequence_info);
            } else {
              sequence_infos.try_emplace(std::string(*publisher_node_id),
                                         std::move(new_sequence_info));
            }
            lck.unlock();

            if (p->get_str_attr("type") == "subscription_update") {
              update_subscriptions(std::move(*p));
            } else {
              if (p->has_attr("_internal_forwarded_by")) {
                p->set_attr(
                    "_internal_forwarded_by",
                    std::string(*p->get_str_attr("_internal_forwarded_by")) +
                        "," + partner_node_id);
              } else {
                p->set_attr("_internal_forwarded_by", partner_node_id);
              }
              PubSub::Router::get_instance().publish(std::move(*p));
            }
#if CONFIG_BENCHMARK_ENABLED
            current_traffic.num_accepted_messages++;
            current_traffic.size_accepted_messages += publicaton_full_size;
#endif
          } else {
            Support::Logger::trace("REMOTE-CONNECTION", "RECEIVE",
              "Message Dropped from %s, forwarded-by: %s",
              publisher_node_id->data(),
              partner_node_id.data()
            );
#if CONFIG_BENCHMARK_ENABLED
            current_traffic.num_duplicate_messages++;
            current_traffic.size_duplicate_messages += publicaton_full_size;
#endif
          }
        }
        publication_buffer.clear();
        publication_buffer.shrink_to_fit();
        state = empty;
        assert(publicaton_remaining_bytes == 0);
      }
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
          subscription_ids.push_back(sub_id);
          uActor::Support::Logger::trace("REMOTE", "SUB-ID ADDED", "%d",
                                         sub_id);
        }
      }
    }
    for (auto subscription_id : old_subscriptions) {
      handle->remove_subscription(local_id, subscription_id, partner_node_id);
      subscription_ids.remove(subscription_id);
      uActor::Support::Logger::trace("REMOTE", "SUB-ID REMOVED", "%d",
                                     subscription_id);
    }
  }
}

#if CONFIG_BENCHMARK_ENABLED
ConnectionTraffic RemoteConnection::current_traffic; 
#endif

}  // namespace uActor::Remote
