#pragma once

extern "C" {
#include <arpa/inet.h>
}

#include <atomic>
#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "forwarder_api.hpp"
#include "forwarding_strategy.hpp"
#include "pubsub/publication_factory.hpp"
#include "remote/sequence_info.hpp"

namespace uActor::Remote {

class TCPForwarder;

enum struct ConnectionRole : uint8_t { SERVER = 0, CLIENT = 1 };

class RemoteConnection {
 public:
  RemoteConnection(const RemoteConnection&) = delete;
  RemoteConnection& operator=(const RemoteConnection&) = delete;
  RemoteConnection(RemoteConnection&&) = default;
  RemoteConnection& operator=(RemoteConnection&&) = default;

  RemoteConnection(uint32_t local_id, int32_t socket_id,
                   std::string remote_addr, uint16_t remote_port,
                   ConnectionRole connection_role,
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

 private:
  uint32_t local_id;
  int sock = 0;

  // TCP Related
  // TODO(raphaelhetzel) potentially move this to a separate
  // wrapper once we have more types of forwarders
  std::string partner_ip;
  uint16_t partner_port;
  ConnectionRole connection_role;

  // Subscription related
  ForwarderSubscriptionAPI* handle;
  std::set<uint32_t> subscription_ids;
  uint32_t update_sub_id = 0;
  uint32_t add_sub_id = 0;
  uint32_t remove_sub_id = 0;

  // Peer related
  std::string partner_node_id;
  uint32_t last_read_contact = 0;
  uint32_t last_write_contact = 0;

  // Connection statemachine
  ProcessingState state{empty};

  int len = 0;
  std::vector<char> rx_buffer = std::vector<char>(512);

  // The size field may be split into multiple recvs
  char size_buffer[4]{0, 0, 0, 0};
  uint32_t size_field_remaining_bytes{0};

  uint32_t publicaton_remaining_bytes{0};
  uint32_t publicaton_full_size{0};
  PubSub::PublicationFactory publication_buffer = PubSub::PublicationFactory();

  std::unique_ptr<ForwardingStrategy> forwarding_strategy;

  std::queue<std::shared_ptr<std::vector<char>>> write_buffer;
  size_t write_offset = 0;

  void process_publication(PubSub::Publication&& p);

  void update_subscriptions(PubSub::Publication&& p);
  void add_subscriptions(PubSub::Publication&& p);
  void remove_subscriptions(PubSub::Publication&& p);

  friend uActor::Remote::TCPForwarder;
  friend uActor::Remote::TCPForwarder;
};

}  // namespace uActor::Remote
