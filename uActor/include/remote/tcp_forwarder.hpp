#pragma once

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "board_functions.hpp"
#include "forwarder_api.hpp"
#include "pubsub/router.hpp"
#include "remote_connection.hpp"

namespace uActor::Remote {

struct TCPAddressArguments {
  TCPAddressArguments(std::string listen_ip, uint16_t port,
                      std::string external_address, uint16_t external_port)
      : listen_ip(listen_ip),
        port(port),
        external_address_hint(external_address),
        external_port_hint(external_port) {}

  std::string listen_ip;
  uint16_t port;

  std::string external_address_hint;
  uint16_t external_port_hint;

  void* tcp_forwarder = nullptr;
};

class TCPForwarder : public uActor::Remote::ForwarderSubscriptionAPI {
 public:
  constexpr static const char* TAG = "tcp_forwarder";

  explicit TCPForwarder(TCPAddressArguments address_arguments);

  static void os_task(void* args);

  static void tcp_reader_task(void* args);

  void receive(PubSub::MatchedPublication&& m);

  uint32_t add_remote_subscription(uint32_t local_id, PubSub::Filter&& filter,
                                   std::string node_id) override;
  void remove_remote_subscription(uint32_t local_id, uint32_t sub_id,
                                  std::string node_id) override;

  uint32_t add_local_subscription(uint32_t local_id,
                                  PubSub::Filter&& filter) override;
  void remove_local_subscription(uint32_t local_id, uint32_t sub_id) override;

 private:
  int forwarder_subscription_id;
  int peer_announcement_subscription_id;
  int subscription_update_subscription_id;
  PubSub::ReceiverHandle handle;
  TCPAddressArguments _address_arguments;

  uint32_t next_local_id = 0;
  std::unordered_map<uint32_t, uActor::Remote::RemoteConnection> remotes;
  std::map<uint32_t, std::set<uint32_t>> subscription_mapping;

  std::mutex remote_mtx;

  int listen_sock = 0;

  std::pair<bool, std::unique_lock<std::mutex>> write(
      RemoteConnection* remote, std::shared_ptr<std::vector<char>> dataset,
      std::unique_lock<std::mutex>&& lock);

  void tcp_reader();

  void create_tcp_client(std::string_view peer_ip, uint32_t port);

  void listen_handler();

  bool data_handler(uActor::Remote::RemoteConnection* remote);

  bool write_handler(uActor::Remote::RemoteConnection* remote);

  void keepalive();

  void add_remote_connection(int socket_id, std::string remote_addr,
                             uint16_t remote_port,
                             uActor::Remote::ConnectionRole role);

  static void set_socket_options(int socket_id);
};

}  // namespace uActor::Remote
