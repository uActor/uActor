#ifndef MAIN_INCLUDE_REMOTE_CONNECTION_HPP_
#define MAIN_INCLUDE_REMOTE_CONNECTION_HPP_

extern "C" {
#ifdef ESP32
#include <lwip/def.h>
#else
#include <arpa/inet.h>
#endif
}

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "board_functions.hpp"
#include "subscription.hpp"

class TCPForwarder;

struct ForwarderSubscriptionAPI {
  virtual uint32_t add_subscription(uint32_t local_id, PubSub::Filter&& filter,
                                    std::string_view node_id) = 0;
  virtual void remove_subscription(uint32_t local_id, uint32_t sub_id,
                                   std::string_view node_id) = 0;
  virtual bool write(int sock, int len, const char* message) = 0;
  virtual ~ForwarderSubscriptionAPI() {}
};

class RemoteConnection {
 public:
  RemoteConnection(const RemoteConnection&) = delete;
  RemoteConnection& operator=(const RemoteConnection&) = delete;
  RemoteConnection(RemoteConnection&&) = default;
  RemoteConnection& operator=(RemoteConnection&&) = default;

  RemoteConnection(uint32_t local_id, int32_t sock,
                   ForwarderSubscriptionAPI* handle)
      : sock(sock), local_id(local_id), handle(handle) {}

  ~RemoteConnection() {
    for (const auto& subscription_id : subscription_ids) {
      handle->remove_subscription(local_id, subscription_id, std::string(""));
    }
    subscription_ids.clear();
    if (!partner_node_id.empty()) {
      Publication update{BoardFunctions::NODE_ID, "remote_connection",
                         std::to_string(local_id)};
      update.set_attr("type", "peer_update");
      update.set_attr("update_type", "peer_removed");
      update.set_attr("peer_node_id", partner_node_id);
      PubSub::Router::get_instance().publish(std::move(update));
    }
  }

  enum ProcessingState {
    empty,
    waiting_for_size,
    waiting_for_data,
  };

  void handle_subscription_update_notification(
      const Publication& update_message) {
    if (update_message.get_str_attr("node_id") == BoardFunctions::NODE_ID) {
      send_routing_info();
    }
  }

  void send_routing_info() {
    Publication p{BoardFunctions::NODE_ID, "remote_connection",
                  std::to_string(local_id)};
    p.set_attr("type", "subscription_update");
    p.set_attr("subscription_node_id", std::string(BoardFunctions::NODE_ID));
    p.set_attr("_internal_sequence_number",
               static_cast<int32_t>(RemoteConnection::sequence_number++));
    p.set_attr("_internal_epoch", static_cast<int32_t>(BoardFunctions::epoch));
    if (!partner_node_id.empty()) {
      p.set_attr(
          "serialized_subscriptions",
          PubSub::Router::get_instance().subscriptions_for(partner_node_id));
    }

    std::string serialized = p.to_msg_pack();
    uint32_t size = htonl(serialized.size());
    handle->write(sock, 4, reinterpret_cast<char*>(&size));
    handle->write(sock, serialized.size(), serialized.data());
  }

  void process_data(uint32_t len, char* data) {
    uint32_t bytes_remaining = len;
    while (bytes_remaining > 0) {
      if (state == empty) {
        // testbed_start_timekeeping((std::string("receive") +
        // std::to_string(local_id)).data());
        if (bytes_remaining >= 4) {
          publicaton_full_size = ntohl(
              *reinterpret_cast<uint32_t*>(data + (len - bytes_remaining)));
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
                      data + (len - bytes_remaining),
                      size_field_remaining_bytes);
          publicaton_full_size =
              ntohl(*reinterpret_cast<uint32_t*>(size_buffer));
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
        uint32_t to_move =
            std::min(publicaton_remaining_bytes, bytes_remaining);
        std::memcpy(publication_buffer.data() +
                        (publicaton_full_size - publicaton_remaining_bytes),
                    data + (len - bytes_remaining), to_move);
        bytes_remaining -= to_move;
        publicaton_remaining_bytes -= to_move;
        if (publicaton_remaining_bytes == 0) {
          // testbed_stop_timekeeping((std::string("receive") +
          // std::to_string(local_id)).data());
          auto p = Publication::from_msg_pack(std::string_view(
              publication_buffer.data(), publicaton_full_size));
          if (p && p->has_attr("publisher_node_id") &&
              p->get_str_attr("publisher_node_id") != BoardFunctions::NODE_ID) {
            auto publisher_node_id = p->get_str_attr("publisher_node_id");
            auto sequence_number = p->get_int_attr("_internal_sequence_number");
            auto epoch_number = p->get_int_attr("_internal_epoch");

            std::unique_lock lck(mtx);
            auto sequence_info_it =
                sequence_infos.find(std::string(*publisher_node_id));
            if (sequence_info_it == sequence_infos.end() ||
                (epoch_number && sequence_number &&
                 (*epoch_number > sequence_info_it->second.epoch ||
                  (*epoch_number == sequence_info_it->second.epoch &&
                   *sequence_number >
                       sequence_info_it->second.sequence_number)))) {
              auto new_sequence_info = SequenceInfo(
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

            } else {
              printf("message dropped %s\n", publisher_node_id->data());
            }
          }
          publication_buffer.clear();
          publication_buffer.resize(0);
          state = empty;
          assert(publicaton_remaining_bytes == 0);
        } else {
        }
      }
    }
  }

  static std::atomic<uint32_t> sequence_number;

 private:
  struct SequenceInfo {
    SequenceInfo(uint32_t sequence_number, uint32_t epoch,
                 uint32_t last_timestamp)
        : sequence_number(sequence_number),
          epoch(epoch),
          last_timestamp(last_timestamp) {}

    uint32_t sequence_number;
    uint32_t epoch;
    uint32_t last_timestamp;
  };

  // TCP Related
  // TODO(raphaelhetzel) potentially move this to a seperate
  // wrapper once we have more types of forwarders
  int sock{0};
  int len{0};
  std::vector<char> rx_buffer = std::vector<char>(512);

  // Subscription related
  uint32_t local_id;
  ForwarderSubscriptionAPI* handle;
  std::list<uint32_t> subscription_ids;

  std::string partner_node_id;

  // Connection statemachine
  ProcessingState state{empty};

  // The size field may be split into multiple recvs
  char size_buffer[4]{0, 0, 0, 0};
  uint32_t size_field_remaining_bytes{0};

  uint32_t publicaton_remaining_bytes{0};
  uint32_t publicaton_full_size{0};
  std::vector<char> publication_buffer;

  static std::unordered_map<std::string, SequenceInfo> sequence_infos;
  static std::mutex mtx;

  void update_subscriptions(Publication&& p) {
    std::string_view new_partner_id = *p.get_str_attr("subscription_node_id");

    if (partner_node_id.empty()) {
      Publication update{BoardFunctions::NODE_ID, "remote_connection",
                         std::to_string(local_id)};
      update.set_attr("type", "peer_update");
      update.set_attr("update_type", "peer_added");
      update.set_attr("peer_node_id", new_partner_id);
      PubSub::Router::get_instance().publish(std::move(update));

      partner_node_id = std::string(new_partner_id);

      // TODO(raphaelhetzel) We could reduce traffic a bit by delaying one of
      // the parties.
      send_routing_info();

      // Flooding
      // subscription_ids.push_back(handle->add_subscription(local_id,
      // PubSub::Filter{}, std::string_view(partner_node_id)));
    }

    if (p.has_attr("serialized_subscriptions")) {
      std::unordered_set<uint32_t> old_subscriptions =
          std::unordered_set<uint32_t>(subscription_ids.begin(),
                                       subscription_ids.end());
      printf("received external subs\n");
      for (auto serialized : StringHelper::string_split(
               *p.get_str_attr("serialized_subscriptions"), "&")) {
        auto deserialized = PubSub::Filter::deserialize(serialized);
        if (deserialized) {
          uint32_t sub_id =
              handle->add_subscription(local_id, std::move(*deserialized),
                                       std::string_view(partner_node_id));
          if (old_subscriptions.erase(sub_id) == 0) {
            subscription_ids.push_back(sub_id);
          }
        }
      }
      for (auto subscription_id : old_subscriptions) {
        handle->remove_subscription(local_id, subscription_id,
                                    std::string_view(partner_node_id));
      }
    }
  }
  friend TCPForwarder;
};
#endif  // MAIN_INCLUDE_REMOTE_CONNECTION_HPP_
