#ifndef UACTOR_INCLUDE_REMOTE_REMOTE_CONNECTION_HPP_
#define UACTOR_INCLUDE_REMOTE_REMOTE_CONNECTION_HPP_

extern "C" {
#include <arpa/inet.h>
}

#include <atomic>
#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "forwarder_api.hpp"
#include "remote/sequence_info.hpp"

namespace uActor::ESP32::Remote {
class TCPForwarder;
}  // namespace uActor::ESP32::Remote

namespace uActor::Linux::Remote {
class TCPForwarder;
}  // namespace uActor::Linux::Remote

namespace uActor::Remote {

class RemoteConnection {
 public:
  RemoteConnection(const RemoteConnection&) = delete;
  RemoteConnection& operator=(const RemoteConnection&) = delete;
  RemoteConnection(RemoteConnection&&) = default;
  RemoteConnection& operator=(RemoteConnection&&) = default;

  RemoteConnection(uint32_t local_id, int32_t sock,
                   ForwarderSubscriptionAPI* handle);

  ~RemoteConnection();

  enum ProcessingState {
    empty,
    waiting_for_size,
    waiting_for_data,
  };

  void handle_subscription_update_notification(
      const PubSub::Publication& update_message);

  void send_routing_info();

  void process_data(uint32_t len, char* data);

  static std::atomic<uint32_t> sequence_number;

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
  uint32_t update_sub_id;

  std::string partner_node_id;

  // Connection statemachine
  ProcessingState state{empty};

  // The size field may be split into multiple recvs
  char size_buffer[4]{0, 0, 0, 0};
  uint32_t size_field_remaining_bytes{0};

  uint32_t publicaton_remaining_bytes{0};
  uint32_t publicaton_full_size{0};
  std::vector<char> publication_buffer;

  static std::unordered_map<std::string, Remote::SequenceInfo> sequence_infos;
  static std::mutex mtx;

  std::map<std::string, Remote::SequenceInfo> connection_sequence_infos;

  void update_subscriptions(PubSub::Publication&& p);
  friend uActor::ESP32::Remote::TCPForwarder;
  friend uActor::Linux::Remote::TCPForwarder;
};

}  // namespace uActor::Remote

#endif  // UACTOR_INCLUDE_REMOTE_REMOTE_CONNECTION_HPP_
