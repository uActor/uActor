#ifndef MAIN_LINUX_INCLUDE_REMOTE_TCP_FORWARDER_HPP_
#define MAIN_LINUX_INCLUDE_REMOTE_TCP_FORWARDER_HPP_

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

#include "board_functions.hpp"
#include "pubsub/router.hpp"
#include "remote/forwarder_api.hpp"
#include "remote/remote_connection.hpp"

namespace uActor::Linux::Remote {

struct TCPTaskArgs {
  TCPTaskArgs(std::string listen_ip, uint32_t port)
      : listen_ip(listen_ip), port(port) {}
  std::string listen_ip;
  int port;
};

class TCPForwarder : public uActor::Remote::ForwarderSubscriptionAPI {
 public:
  constexpr static const char* TAG = "tcp_forwarder";

  explicit TCPForwarder(std::string listen_ip, uint32_t port);

  static void os_task(void* args);

  static void tcp_reader_task(void* args);

  void receive(PubSub::MatchedPublication&& m);

  uint32_t add_subscription(uint32_t local_id, PubSub::Filter&& filter,
                            std::string_view node_id);

  void remove_subscription(uint32_t local_id, uint32_t sub_id,
                           std::string_view node_id);

 private:
  int forwarder_subscription_id;
  int peer_announcement_subscription_id;
  int subscription_update_subscription_id;
  uint32_t _port;
  std::string _listen_ip;
  PubSub::ReceiverHandle handle;

  uint32_t next_local_id = 0;
  std::unordered_map<uint32_t, uActor::Remote::RemoteConnection> remotes;
  std::map<uint32_t, std::set<uint32_t>> subscription_mapping;

  std::mutex remote_mtx;
  int listen_sock;

  bool write(int sock, int len, const char* message);

  void tcp_reader();

  void create_tcp_client(std::string_view peer_ip, uint32_t port);

  void listen_handler();

  void data_handler(uActor::Remote::RemoteConnection* remote);
};

}  // namespace uActor::Linux::Remote

#endif  //  MAIN_LINUX_INCLUDE_REMOTE_TCP_FORWARDER_HPP_
