#include "remote/tcp_forwarder.hpp"

#ifdef ESP_IDF
#include <sdkconfig.h>

extern "C" {
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
}
#else
extern "C" {
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
}
#include <thread>
#endif

#if CONFIG_BENCHMARK_ENABLED
#include <support/testbed.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "board_functions.hpp"
#include "support/logger.hpp"

namespace uActor::Remote {

using uActor::Support::Logger;

TCPForwarder::TCPForwarder(TCPAddressArguments address_arguments)
    : handle(PubSub::Router::get_instance().new_subscriber()),
      _address_arguments(address_arguments) {
  PubSub::Filter primary_filter{
      PubSub::Constraint(std::string("node_id"), BoardFunctions::NODE_ID),
      PubSub::Constraint(std::string("actor_type"), "forwarder"),
      PubSub::Constraint(std::string("instance_id"), "1")};
  forwarder_subscription_id = handle.subscribe(primary_filter);

  PubSub::Filter peer_announcements{
      PubSub::Constraint(std::string("type"), "tcp_client_connect"),
      PubSub::Constraint(std::string("node_id"), BoardFunctions::NODE_ID),
  };
  peer_announcement_subscription_id = handle.subscribe(peer_announcements);

  PubSub::Filter subscription_update{
      PubSub::Constraint(std::string("type"), "local_subscription_update"),
      PubSub::Constraint(std::string("publisher_node_id"),
                         BoardFunctions::NODE_ID),
  };
  subscription_update_subscription_id = handle.subscribe(subscription_update);
}

void TCPForwarder::os_task(void* args) {
  TCPAddressArguments* task_args = reinterpret_cast<TCPAddressArguments*>(args);
  TCPForwarder fwd = TCPForwarder(*task_args);
  task_args->tcp_forwarder = &fwd;
  while (true) {
    auto result = fwd.handle.receive(10000);
    if (result) {
      fwd.receive(std::move(*result));
    }
    fwd.keepalive();
  }
}

void TCPForwarder::tcp_reader_task(void* args) {
  TCPForwarder* fwd = reinterpret_cast<TCPForwarder*>(args);
  fwd->tcp_reader();
}

void TCPForwarder::receive(PubSub::MatchedPublication&& m) {
  Logger::trace("TCP-FORWARDER", "ACTOR-RECEIVE", "Begin");
  std::unique_lock remote_lock(remote_mtx);

  if (m.subscription_id == forwarder_subscription_id) {
    Logger::warning("TCP-FORWARDER", "ACTOR-RECEIVE",
                    "Forwarder received unhandled message!");
    return;
  }

  if (m.subscription_id == subscription_update_subscription_id) {
    if (m.publication.get_str_attr("type") == "local_subscription_update") {
      for (auto& receiver : remotes) {
        receiver.second.handle_subscription_update_notification(m.publication);
      }
    }
  }

  if (m.subscription_id == peer_announcement_subscription_id) {
    if (m.publication.get_str_attr("type") == "tcp_client_connect" &&
        m.publication.get_str_attr("node_id") == BoardFunctions::NODE_ID) {
      if (m.publication.get_str_attr("peer_node_id") !=
          BoardFunctions::NODE_ID) {
        std::string peer_ip = std::string(
            std::get<std::string_view>(m.publication.get_attr("peer_ip")));
        uint32_t peer_port =
            std::get<int32_t>(m.publication.get_attr("peer_port"));
        create_tcp_client(peer_ip, peer_port);
      }
    }
  }

  auto receivers = subscription_mapping.find(m.subscription_id);
  if (receivers != subscription_mapping.end()) {
    auto sub_ids = receivers->second;

    std::string serialized = m.publication.to_msg_pack();
    int size = htonl(serialized.size());
    for (uint32_t subscriber_id : sub_ids) {
      auto remote_it = remotes.find(subscriber_id);
      if (remote_it != remotes.end() && remote_it->second.sock > 0) {
        auto& remote = remote_it->second;

        bool skip = false;

        if (m.publication.has_attr("_internal_forwarded_by")) {
          size_t start = 0;
          std::string_view forwarders =
              *m.publication.get_str_attr("_internal_forwarded_by");
          while (start < forwarders.length()) {
            start = forwarders.find(remote.partner_node_id, start);
            if (start != std::string_view::npos) {
              size_t end_pos = start + remote.partner_node_id.length();
              if ((start == 0 || forwarders.at(start - 1) == ',') &&
                  (end_pos == forwarders.length() ||
                   forwarders.at(end_pos) == ',')) {
                skip = true;
                break;
              } else {
                start++;
              }
            } else {
              break;
            }
          }
        }

        // The might be multiple subscriptions for the same message.
        // They are indistinguishable on the remote node. Hence, filter them
        // and prevent forwarding. This is e.g. important usefull for external
        // deployments if the deployment manager use label filters. As this
        // has to be per connection, this unfortunately causes a potentially
        // large memory overhead.
        // TODO(raphaehetzel) Add garbage collector to remove outdated
        // sequence infos. As this is an optimization, this is safe here.

        {
          auto publisher_node_id =
              m.publication.get_str_attr("publisher_node_id");
          auto sequence_number =
              m.publication.get_int_attr("_internal_sequence_number");
          auto epoch_number = m.publication.get_int_attr("_internal_epoch");

          if (publisher_node_id && sequence_number && epoch_number) {
            auto new_sequence_info = uActor::Remote::SequenceInfo(
                *sequence_number, *epoch_number, BoardFunctions::timestamp());
            auto sequence_info_it = remote.connection_sequence_infos.find(
                std::string(*publisher_node_id));
            if (sequence_info_it == remote.connection_sequence_infos.end()) {
              remote.connection_sequence_infos.try_emplace(
                  std::string(*publisher_node_id),
                  std::move(new_sequence_info));
            } else if (sequence_info_it->second.is_older_than(*sequence_number,
                                                              *epoch_number)) {
              sequence_info_it->second = std::move(new_sequence_info);
            } else {
              skip = true;
            }
          }
        }

        if (!skip) {
          if (write(remote.partner_ip, remote.partner_port, remote.sock, 4,
                    reinterpret_cast<char*>(&size)) ||
              write(remote.partner_ip, remote.partner_port, remote.sock,
                    serialized.size(), serialized.c_str())) {
            Logger::info("TCP-FORWARDER", "ACTOR-RECEIVE",
                         "Closing connection - %s:%d",
                         remote_it->second.partner_ip.data(),
                         remote_it->second.partner_port);
            shutdown(remote_it->second.sock, 0);
            close(remote_it->second.sock);
            remotes.erase(remote_it);
            Logger::trace("TCP-FORWARDER", "ACTOR-RECEIVE",
                          "Connection removed");
            continue;
          } else {
            remote.last_write_contact = BoardFunctions::timestamp();
          }
        }
      }
    }
  }
  Logger::trace("TCP-FORWARDER", "ACTOR-RECEIVE", "End");
}

uint32_t TCPForwarder::add_subscription(uint32_t local_id,
                                        PubSub::Filter&& filter,
                                        std::string node_id) {
  uint32_t sub_id = handle.subscribe(std::move(filter), node_id);
  auto entry = subscription_mapping.find(sub_id);
  if (entry != subscription_mapping.end()) {
    entry->second.insert(local_id);
  } else {
    subscription_mapping.emplace(sub_id, std::set<uint32_t>{local_id});
  }
  return sub_id;
}

void TCPForwarder::remove_subscription(uint32_t local_id, uint32_t sub_id,
                                       std::string node_id) {
  auto it = subscription_mapping.find(sub_id);
  if (it != subscription_mapping.end()) {
    it->second.erase(local_id);
    if (it->second.empty()) {
      handle.unsubscribe(sub_id);
      subscription_mapping.erase(it);
    }
  }
}

bool TCPForwarder::write(std::string remote_ip, uint16_t remote_port,
                         int socket, int len, const char* message) {
  int to_write = len;
  while (to_write > 0) {
#if defined(MSG_NOSIGNAL)  // POSIX
    int flag = MSG_NOSIGNAL;
#elif defined(SO_NOSIGPIPE)  // MacOS
    int flag = SO_NOSIGPIPE;
#endif
    int written = send(socket, message + (len - to_write), to_write, flag);
    if (written < 0) {
      Logger::info("TCP-FORWARDER", "WRITE",
                   "Error occurred during send - %s:%d - error %d",
                   remote_ip.c_str(), remote_port, errno);
      return true;
    }
    to_write -= written;
  }
  return false;
}

void TCPForwarder::tcp_reader() {
  fd_set sockets_to_read;

  int addr_family = AF_INET;
  int ip_protocol = IPPROTO_IP;

  sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = inet_addr(_address_arguments.listen_ip.data());
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(_address_arguments.port);

  listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (listen_sock < 0) {
    Logger::warning("TCP-FORWARDER", "SERVER",
                    "Unable to create socket - error %d", errno);
    return;
  }

  int reuse = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, static_cast<void*>(&reuse),
             sizeof(reuse));
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, static_cast<void*>(&reuse),
             sizeof(reuse));

  int err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
  if (err != 0) {
    Logger::error("TCP-FORWARDER", "SERVER",
                  "Socket unable to bind - error %d\n", errno);
    close(listen_sock);
    listen_sock = 0;
  }

  Logger::info("TCP-FORWARDER", "SERVER", "Socket bound: %s:%d\n",
               _address_arguments.listen_ip.data(), _address_arguments.port);

  if (_address_arguments.external_port_hint > 0 ||
      _address_arguments.external_address_hint.length() > 0) {
    std::string external_address =
        _address_arguments.external_address_hint.length() > 0
            ? _address_arguments.external_address_hint
            : _address_arguments.listen_ip;
    uint16_t external_port = _address_arguments.external_port_hint
                                 ? _address_arguments.external_port_hint
                                 : _address_arguments.port;
    Logger::info("TCP-FORWARDER", "SERVER",
                 "TCP Socket externally reachable at: %s:%d",
                 external_address.data(), external_port);
  }

#if CONFIG_BENCHMARK_ENABLED
  if (_address_arguments.external_address_hint.length() > 0) {
    testbed_log_rt_string("address",
                          _address_arguments.external_address_hint.data());
  } else {
    testbed_log_rt_string("address", _address_arguments.listen_ip.data());
  }

  if (_address_arguments.external_port_hint > 0) {
    testbed_log_rt_integer("port", _address_arguments.external_port_hint);
  } else {
    testbed_log_rt_integer("port", _address_arguments.port);
  }
#endif

  err = listen(listen_sock, 1);
  if (err != 0) {
    Logger::error("TCP-FORWARDER", "SERVER", "Socket listen error - %d", errno);
    close(listen_sock);
    listen_sock = 0;
  }

  while (1) {
    std::unique_lock remote_lock(remote_mtx);
    FD_ZERO(&sockets_to_read);
    FD_SET(listen_sock, &sockets_to_read);
    int max_val = listen_sock;
    for (const auto& remote_pair : remotes) {
      FD_SET(remote_pair.second.sock, &sockets_to_read);
      max_val = std::max(max_val, remote_pair.second.sock);
    }
    timeval timeout;  // We need to set a timeout to allow for new connections
    timeout.tv_sec = 5;  // to be added by other threads

    remote_lock.unlock();
    int num_ready =
        select(max_val + 1, &sockets_to_read, nullptr, nullptr, &timeout);
    remote_lock.lock();

    Logger::trace("TCP-FORWARDER", "EVENT-LOOP", "TICK");

    if (num_ready > 0) {
      std::vector<uint> to_delete;
      for (const auto& remote_pair : remotes) {
        uActor::Remote::RemoteConnection& remote =
            const_cast<uActor::Remote::RemoteConnection&>(remote_pair.second);
        if (remote.sock > 0 && FD_ISSET(remote.sock, &sockets_to_read)) {
          data_handler(&remote);
          if (!remote.sock) {
            to_delete.push_back(remote_pair.first);
          }
        } else if (remote.sock < 1) {
          Logger::error("TCP-FORWARDER", "RECEIVE",
                        "Socket should have already been deleted %s:%d!",
                        remote.partner_ip.data(), remote.partner_port);
        }
      }
      for (auto item : to_delete) {
        remotes.erase(item);
      }
      if (FD_ISSET(listen_sock, &sockets_to_read)) {
        listen_handler();
      }
    }
  }
}

void TCPForwarder::create_tcp_client(std::string_view peer_ip, uint32_t port) {
  Logger::trace("TCP-FORWARDER", "CLIENT", "Begin");
  int addr_family = AF_INET;
  int ip_protocol = IPPROTO_IP;

  sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = inet_addr(peer_ip.data());
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(port);

  int sock_id = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (sock_id < 0) {
    Logger::error("TCP-FORWARDER", "CLIENT", "Socker create error - %d", errno);
    return;
  }
  int err = connect(sock_id, reinterpret_cast<sockaddr*>(&dest_addr),
                    sizeof(dest_addr));
  if (err != 0) {
    Logger::error("TCP-FORWARDER", "CLIENT",
                  "Socket connect error - %s:%d - %d", peer_ip.data(), port,
                  errno);
    return;
  }
  Logger::info("TCP-FORWARDER", "CLIENT", "Socket connected - %s:%d",
               peer_ip.data(), port);

  set_socket_options(sock_id);
  add_remote_connection(sock_id, std::string(peer_ip), port,
                        uActor::Remote::ConnectionRole::CLIENT);
  Logger::trace("TCP-FORWARDER", "CLIENT", "End");
}

void TCPForwarder::listen_handler() {
  Logger::trace("TCP-FORWARDER", "SERVER", "Begin");
  int sock_id = 0;

  struct sockaddr_in6 source_addr;
  uint addr_len = sizeof(source_addr);
  sock_id =
      accept(listen_sock, reinterpret_cast<sockaddr*>(&source_addr), &addr_len);

  if (sock_id < 0) {
    Logger::error("TCP-FORWARDER", "SERVER",
                  "Unable to accept connection - error %d", errno);
    return;
  }

  char addr_string[INET6_ADDRSTRLEN];
  uint16_t remote_port = 0;
  if (source_addr.sin6_family == PF_INET) {
    auto addr = reinterpret_cast<sockaddr_in*>(&source_addr);
    inet_ntop(addr->sin_family, &addr->sin_addr, addr_string, INET_ADDRSTRLEN);
    remote_port = ntohs(addr->sin_port);
  } else if (source_addr.sin6_family == PF_INET6) {
    inet_ntop(source_addr.sin6_family, &source_addr.sin6_addr, addr_string,
              INET6_ADDRSTRLEN);
    remote_port = ntohs(source_addr.sin6_port);
  }
  Logger::info("TCP-FORWARDER", "SERVER", "Connection accepted - %s:%d",
               addr_string, remote_port);

  set_socket_options(sock_id);
  add_remote_connection(sock_id, std::string(addr_string), remote_port,
                        uActor::Remote::ConnectionRole::SERVER);
  Logger::trace("TCP-FORWARDER", "SERVER", "End");
}

void TCPForwarder::data_handler(uActor::Remote::RemoteConnection* remote) {
  Logger::trace("TCP-FORWARDER", "RECEIVE", "Begin");
  remote->last_read_contact = BoardFunctions::timestamp();
  remote->len =
      recv(remote->sock, remote->rx_buffer.data(), remote->rx_buffer.size(), 0);
  if (remote->len < 0) {
    Logger::error("TCP-FORWARDER", "RECEIVE",
                  "Error occurred during receive - %d", errno);
  } else if (remote->len == 0) {
    Logger::info("TCP-FORWARDER", "RECEIVE", "Connection closed - %s:%d",
                 remote->partner_ip.data(), remote->partner_port);
  } else {
    remote->process_data(remote->len, remote->rx_buffer.data());
    Logger::trace("TCP-FORWARDER", "RECEIVE", "End - Good Case");
    return;
  }

  Logger::info("TCP-FORWARDER", "RECEIVE", "Closing connection - %s:%d",
               remote->partner_ip.data(), remote->partner_port);
  shutdown(remote->sock, 0);
  close(remote->sock);
  remote->sock = 0;
  Logger::trace("TCP-FORWARDER", "RECEIVE", "End - Error Case");
}

void TCPForwarder::keepalive() {
  Logger::trace("TCP-FORWARDER", "KEEPALIVE", "Begin");
  std::set<uint32_t> to_delete;
  std::unique_lock lock(remote_mtx);
  for (auto& remote : remotes) {
    uint32_t reference = BoardFunctions::timestamp();
    uint32_t zero_len = 0;
    if (reference - remote.second.last_write_contact > 10000) {
      if (write(remote.second.partner_ip, remote.second.partner_port,
                remote.second.sock, sizeof(uint32_t),
                reinterpret_cast<char*>(&zero_len))) {
        Logger::info("TCP-FORWARDER", "KEEPALIVE", "Write check failed");
        shutdown(remote.second.sock, 0);
        close(remote.second.sock);
        to_delete.insert(remote.first);
      } else {
        remote.second.last_write_contact = BoardFunctions::timestamp();
      }
    }

    if (reference - remote.second.last_read_contact > 30000 &&
        remote.second.last_read_contact > 0) {
      Logger::info("TCP-FORWARDER", "KEEPALIVE", "Read check failed");
      shutdown(remote.second.sock, 0);
      close(remote.second.sock);
      to_delete.insert(remote.first);
    }
  }

  for (auto inactive_node : to_delete) {
    Logger::trace("TCP-Forwarder", "KEEPALIVE", "Delete remote");
    remotes.erase(inactive_node);
  }
  Logger::trace("TCP-FORWARDER", "KEEPALIVE", "End");
}

void TCPForwarder::add_remote_connection(int socket_id, std::string remote_addr,
                                         uint16_t remote_port,
                                         uActor::Remote::ConnectionRole role) {
  uint32_t remote_id = next_local_id++;
  if (auto [remote_it, inserted] =
          remotes.try_emplace(remote_id, remote_id, socket_id, remote_addr,
                              remote_port, role, this);
      inserted) {
    if (role == uActor::Remote::ConnectionRole::CLIENT) {
      remote_it->second.send_routing_info();
    }
    Logger::trace("TCP-FORWARDER", "ADD-CONNECTION", "End - Good Case");
  } else {
    Logger::warning("TCP-FORWARDER", "ADD-CONNECTION",
                    "Remote not inserted, closing "
                    "connection - "
                    "%s:%d!",
                    remote_addr.c_str(), remote_port);
    shutdown(socket_id, 0);
    close(socket_id);
    Logger::trace("TCP-FORWARDER", "ADD-CONNECTION", "End - Error Case");
  }
}

void TCPForwarder::set_socket_options(int socket_id) {
  int nodelay = 1;
  setsockopt(socket_id, IPPROTO_TCP, TCP_NODELAY, static_cast<void*>(&nodelay),
             sizeof(nodelay));

  uint32_t true_flag = 1;
  uint32_t keepcnt = 3;
  uint32_t keepidle = 60;
  uint32_t keepintvl = 30;
  setsockopt(socket_id, SOL_SOCKET, SO_KEEPALIVE, &true_flag, sizeof(uint32_t));
  setsockopt(socket_id, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(uint32_t));
  setsockopt(socket_id, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(uint32_t));
  setsockopt(socket_id, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl,
             sizeof(uint32_t));
}
}  // namespace uActor::Remote
