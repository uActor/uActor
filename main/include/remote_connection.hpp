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
#include <string>
#include <utility>
#include <vector>

#include "board_functions.hpp"
#include "subscription.hpp"

class TCPForwarder;

struct ForwarderSubscriptionAPI {
  virtual uint32_t add_subscription(uint32_t local_id,
                                    PubSub::Filter&& filter) = 0;
  virtual void remove_subscription(uint32_t local_id, uint32_t sub_id) = 0;
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
      handle->remove_subscription(local_id, subscription_id);
    }
    subscription_ids.clear();
  }

  enum ProcessingState {
    empty,
    waiting_for_size,
    waiting_for_data,
  };

  void send_routing_info() {
    Publication p{BoardFunctions::NODE_ID, "remote_connection",
                  std::to_string(local_id)};
    p.set_attr("type", "subscription_update");
    p.set_attr("subscription_node_id", BoardFunctions::NODE_ID);

    std::string serialized = p.to_msg_pack();
    uint32_t size = htonl(serialized.size());
    handle->write(sock, 4, reinterpret_cast<char*>(&size));
    handle->write(sock, serialized.size(), serialized.data());
  }

  void process_data(uint32_t len, char* data) {
    uint32_t bytes_remaining = len;
    while (bytes_remaining > 0) {
      if (state == empty) {
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
          auto p = Publication::from_msg_pack(std::string_view(
              publication_buffer.data(), publicaton_full_size));
          if (p) {
            if (p->has_attr("type") && std::get<std::string_view>(p->get_attr(
                                           "type")) == "subscription_update") {
              update_subscriptions(std::move(*p));
            } else {
              p->set_attr("_internal_forwarded_by",
                          partner_node_id);  // TODO(raphaelhetzel) deal with
                                             // cycles with length > 1
              PubSub::Router::get_instance().publish(std::move(*p));
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

 private:
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

  void update_subscriptions(Publication&& p) {
    for (const auto& subscription_id : subscription_ids) {
      handle->remove_subscription(local_id, subscription_id);
    }
    subscription_ids.clear();
    partner_node_id = std::string(
        std::get<std::string_view>(p.get_attr("subscription_node_id")));
    subscription_ids.push_back(
        handle->add_subscription(local_id, PubSub::Filter{}));
  }
  friend TCPForwarder;
};
#endif  // MAIN_INCLUDE_REMOTE_CONNECTION_HPP_
