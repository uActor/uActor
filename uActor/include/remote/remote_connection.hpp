#pragma once

#include <atomic>
#include <cstdint>
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
#include "remote/sequence_info.hpp"
#include "subscription_processors/subscription_processor.hpp"

namespace uActor::Remote {

class TCPForwarder;

enum struct ConnectionRole : uint8_t { SERVER = 0, CLIENT = 1 };

class RemoteConnection {
 public:
  RemoteConnection(const RemoteConnection&) = delete;
  RemoteConnection& operator=(const RemoteConnection&) = delete;
  RemoteConnection(RemoteConnection&&) = default;
  RemoteConnection& operator=(RemoteConnection&&) = default;

  RemoteConnection(uint32_t local_id, ForwarderSubscriptionAPI* handle);

  ~RemoteConnection();

  void send_routing_info();

  void process_data(uint32_t len, char* data);

  static std::map<std::string, std::string> local_location_labels;
  static std::list<std::vector<std::string>> clusters;
  static std::string cluster;
  std::string peer_cluster;

 protected:
  uint32_t local_id;

  // Subscription related
  ForwarderSubscriptionAPI* handle;
  std::set<uint32_t> subscription_ids;
  uint32_t update_sub_id = 0;

  uint32_t local_sub_added_id = 0;
  uint32_t local_sub_removed_id = 0;
  uint32_t remote_subs_added_id = 0;
  uint32_t remote_subs_removed_id = 0;

  // Peer related
  std::string partner_node_id;
  std::map<std::string, std::string> partner_location_labels;

  std::vector<std::unique_ptr<SubscriptionProcessor>>
      egress_subscription_processors;
  std::vector<std::unique_ptr<SubscriptionProcessor>>
      ingress_subscription_processors;

  std::unique_ptr<ForwardingStrategy> forwarding_strategy;

  void process_remote_publication(PubSub::Publication&& p,
                                  size_t encoded_length);

  void handle_local_subscription_removed(const PubSub::Publication& p);
  void handle_local_subscription_added(const PubSub::Publication& p);
  void handle_remote_subscriptions_removed(const PubSub::Publication& p);
  void handle_remote_subscriptions_added(const PubSub::Publication& p);

  void handle_remote_hello(PubSub::Publication&& p);
};

}  // namespace uActor::Remote
