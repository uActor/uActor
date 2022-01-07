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
#include "controllers/telemetry_data.hpp"
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
  forwarder_subscription_id = handle.subscribe(
      primary_filter,
      PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "tcp_forwarder", "1"));

  PubSub::Filter connection_requests{
      PubSub::Constraint(std::string("type"), "tcp_client_connect"),
      PubSub::Constraint(std::string("node_id"), BoardFunctions::NODE_ID),
  };
  peer_announcement_subscription_id = handle.subscribe(
      connection_requests,
      PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "tcp_forwarder", "1"));
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
  std::unique_lock remote_lock(remote_mtx);

  if (m.subscription_id == forwarder_subscription_id) {
    Logger::warning("TCP-FORWARDER", "Received unhandled message!");
    return;
  }

  if (m.publication.get_str_attr("type") == "local_subscription_added") {
    auto s_receivers = subscription_mapping.find(m.subscription_id);
    if (s_receivers != subscription_mapping.end()) {
      auto sub_ids = s_receivers->second;
      for (const auto& sub_id : sub_ids) {
        auto remote_it = remotes.find(sub_id);
        if (remote_it != remotes.end()) {
          remote_it->second.handle_local_subscription_added(m.publication);
        }
      }
    }
    return;
  }

  if (m.publication.get_str_attr("type") == "local_subscription_removed") {
    auto s_receivers = subscription_mapping.find(m.subscription_id);
    if (s_receivers != subscription_mapping.end()) {
      auto sub_ids = s_receivers->second;
      for (const auto& sub_id : sub_ids) {
        auto remote_it = remotes.find(sub_id);
        if (remote_it != remotes.end()) {
          remote_it->second.handle_local_subscription_removed(m.publication);
        }
      }
    }
    return;
  }

  if (m.subscription_id == peer_announcement_subscription_id) {
    Logger::trace("TCP-FORWARDER", "Received peer announcement");
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
    Logger::trace("TCP-FORWARDER",
                  "Received regualar publication. Subscribers: %d",
                  sub_ids.size());

    std::shared_ptr<std::vector<char>> serialized;

    for (uint32_t subscriber_id : sub_ids) {
      auto remote_it = remotes.find(subscriber_id);
      if (remote_it != remotes.end() && remote_it->second.sock > 0) {
        auto& remote = remote_it->second;

        if (remote.forwarding_strategy->should_forward(m.publication)) {
          if (!serialized) {
            serialized = m.publication.to_msg_pack();
            uint32_t size_normal = serialized->size() - 4;
            *reinterpret_cast<uint32_t*>(serialized->data()) =
                htonl(size_normal);
          }

          auto result = write(&remote, serialized, std::move(remote_lock));
          remote_lock = std::move(result.second);
          if (result.first) {
            Logger::info("TCP-FORWARDER",
                         "Receive: Publication write failed - shutdown "
                         "connection - %s:%d",
                         remote_it->second.partner_ip.data(),
                         remote_it->second.partner_port);
            continue;
          }
        }
      }
    }
  }
}

void TCPForwarder::remove_local_subscription(uint32_t local_id,
                                             uint32_t sub_id) {
  auto it = subscription_mapping.find(sub_id);
  if (it != subscription_mapping.end()) {
    it->second.erase(local_id);
    if (it->second.empty()) {
      handle.unsubscribe(sub_id);
      subscription_mapping.erase(it);
    }
  }
}

uint32_t TCPForwarder::add_local_subscription(uint32_t local_id,
                                              PubSub::Filter&& filter) {
  uint32_t sub_id = handle.subscribe(
      std::move(filter),
      PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "tcp_forwarder", "1"));
  auto entry = subscription_mapping.find(sub_id);
  if (entry != subscription_mapping.end()) {
    entry->second.insert(local_id);
  } else {
    subscription_mapping.emplace(sub_id, std::set<uint32_t>{local_id});
  }
  return sub_id;
}

uint32_t TCPForwarder::add_remote_subscription(uint32_t local_id,
                                               PubSub::Filter&& filter,
                                               std::string node_id) {
  uint32_t sub_id =
      handle.subscribe(std::move(filter),
                       PubSub::ActorIdentifier(node_id, "tcp_forwarder", "1"));
  auto entry = subscription_mapping.find(sub_id);
  if (entry != subscription_mapping.end()) {
    entry->second.insert(local_id);
  } else {
    subscription_mapping.emplace(sub_id, std::set<uint32_t>{local_id});
  }
  return sub_id;
}

void TCPForwarder::remove_remote_subscription(uint32_t local_id,
                                              uint32_t sub_id,
                                              std::string /*node_id*/) {
  auto it = subscription_mapping.find(sub_id);
  if (it != subscription_mapping.end()) {
    it->second.erase(local_id);
    if (it->second.empty()) {
      handle.unsubscribe(sub_id);
      subscription_mapping.erase(it);
    }
  }
}

std::pair<bool, std::unique_lock<std::mutex>> TCPForwarder::write(
    TCPConnection* remote, std::shared_ptr<std::vector<char>> dataset,
    std::unique_lock<std::mutex>&& lock) {
  Controllers::TelemetryData::increase("written_traffic_size",
                                       dataset->size() - 4);
  remote->write_buffer.push(std::move(dataset));

#if defined(MSG_NOSIGNAL)  // POSIX
  int flag = MSG_NOSIGNAL | MSG_DONTWAIT;
#elif defined(SO_NOSIGPIPE)  // MacOS
  int flag = SO_NOSIGPIPE | MSG_DONTWAIT;
#endif

  if (!remote->write_buffer.empty()) {
    size_t remaining_size =
        remote->write_buffer.front()->size() - remote->write_offset;
    int written =
        send(remote->sock,
             remote->write_buffer.front()->data() + remote->write_offset,
             remaining_size, flag);
    if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      Logger::debug("TCP-FORWARDER", "Write error");
      shutdown(remote->sock, SHUT_RDWR);
      return std::make_pair(true, std::move(lock));
    } else if (written == remaining_size) {
      Logger::trace("TCP-FORWARDER", "Write completed");
      remote->write_buffer.pop();
      remote->write_offset = 0;
    } else {
      Logger::trace("TCP-FORWARDER", "Write progress");
      if (written > 0) {
        remote->write_offset += written;
      }
      signal_select_change();
    }
    remote->last_write_contact = BoardFunctions::seconds_timestamp();
  }
  return std::make_pair(false, std::move(lock));
}

void TCPForwarder::tcp_reader() {
  fd_set read_sockets;
  fd_set write_sockets;
  fd_set error_sockets;

  int addr_family = AF_INET;
  int ip_protocol = IPPROTO_IP;

  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-member-init, hicpp-member-init)
  sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = inet_addr(_address_arguments.listen_ip.c_str());
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(_address_arguments.port);

  listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (listen_sock < 0) {
    Logger::warning("TCP-FORWARDER",
                    "Unable to create server socket - error %d", errno);
    return;
  }

  int reuse = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, static_cast<void*>(&reuse),
             sizeof(reuse));
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, static_cast<void*>(&reuse),
             sizeof(reuse));

  int err = bind(listen_sock, reinterpret_cast<sockaddr*>(&dest_addr),
                 sizeof(dest_addr));
  if (err != 0) {
    Logger::error("TCP-FORWARDER", "Server socket unable to bind - error %d\n",
                  errno);
    close(listen_sock);
    listen_sock = 0;
  }

  Logger::info("TCP-FORWARDER", "Server socket bound: %s:%d",
               _address_arguments.listen_ip.data(), _address_arguments.port);

  if (_address_arguments.external_port_hint > 0 ||
      _address_arguments.external_address_hint.length() > 0) {
    std::string external_address =
        _address_arguments.external_address_hint.length() > 0
            ? _address_arguments.external_address_hint
            : _address_arguments.listen_ip;
    uint16_t external_port = _address_arguments.external_port_hint > 0
                                 ? _address_arguments.external_port_hint
                                 : _address_arguments.port;
    Logger::info("TCP-FORWARDER",
                 "TCP Server socket externally reachable at: %s:%d",
                 external_address.data(), external_port);
  }

#if CONFIG_BENCHMARK_ENABLED
  if (_address_arguments.external_address_hint.length() > 0) {
    testbed_log_rt_string("address",
                          _address_arguments.external_address_hint.data());
  } else {
    // testbed_log_rt_string("address", _address_arguments.listen_ip.data());
  }

  if (_address_arguments.external_port_hint > 0) {
    testbed_log_rt_integer("port", _address_arguments.external_port_hint);
  } else {
    testbed_log_rt_integer("port", _address_arguments.port);
  }
#endif

  err = listen(listen_sock, 1);
  if (err != 0) {
    Logger::error("TCP-FORWARDER", "Server socket listen error - %d", errno);
    close(listen_sock);
    listen_sock = 0;
  }

  signal_socket = socket(addr_family, SOCK_DGRAM, ip_protocol);
  if (signal_socket < 0) {
    Logger::error("TCP-FORWARDER", "Signal socket creation error - %d", errno);
  }
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-member-init, hicpp-member-init)
  sockaddr_in signal_dest_addr;
  signal_dest_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  signal_dest_addr.sin_family = AF_INET;
  signal_dest_addr.sin_port = htons(_address_arguments.port);
  err = bind(signal_socket, (struct sockaddr*)&signal_dest_addr,
             sizeof(dest_addr));
  if (err != 0) {
    Logger::error("TCP-FORWARDER", "Signal socket bind error - %d", errno);
  } else {
    Logger::debug("TCP-FORWARDER", "Signal socket bound");
  }
  signal_socket_write_handler = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (signal_socket_write_handler < 0) {
    Logger::error("TCP-FORWARDER",
                  "Signal socket write handle creation error - %d", errno);
  }

  while (true) {
    std::unique_lock remote_lock(remote_mtx);
    FD_ZERO(&read_sockets);
    FD_ZERO(&write_sockets);
    FD_ZERO(&error_sockets);
    FD_SET(listen_sock, &read_sockets);
    FD_SET(listen_sock, &write_sockets);
    FD_SET(listen_sock, &error_sockets);
    FD_SET(signal_socket, &read_sockets);
    FD_SET(signal_socket, &error_sockets);
    int max_val = std::max(listen_sock, signal_socket);
    for (const auto& remote_pair : remotes) {
      FD_SET(remote_pair.second.sock, &read_sockets);
      if (!remote_pair.second.write_buffer.empty()) {
        FD_SET(remote_pair.second.sock, &write_sockets);
      }
      FD_SET(remote_pair.second.sock, &error_sockets);
      max_val = std::max(max_val, remote_pair.second.sock);
    }

    timeval timeout{};  // We need to set a timeout to allow for new connections
    timeout.tv_sec = 1;  // to be added by other threads

    remote_lock.unlock();
    int num_ready = select(max_val + 1, &read_sockets, &write_sockets,
                           &error_sockets, &timeout);
    remote_lock.lock();

    if (num_ready > 0) {
      std::vector<uint> to_delete;
      for (auto& remote_pair : remotes) {
        uActor::Remote::TCPConnection& remote = remote_pair.second;
        if (remote.sock > 0) {
          if (FD_ISSET(remote.sock, &error_sockets)) {
            Logger::debug("TCP-FORWARDER", "Event Loop: Error");
            shutdown(remote.sock, SHUT_RDWR);
            close(remote.sock);
            to_delete.push_back(remote_pair.first);
            continue;
          }
          if (FD_ISSET(remote.sock, &read_sockets)) {
            Logger::trace("TCP-FORWARDER", "Event Loop: Read");
            if (data_handler(&remote)) {
              Logger::info(
                  "TCP-FORWARDER",
                  "Read error/zero length message. Closing connection - %s:%d",
                  remote.partner_ip.data(), remote.partner_port);
              shutdown(remote.sock, SHUT_RDWR);
              close(remote.sock);
              to_delete.push_back(remote_pair.first);
              continue;
            }
          }
          if (FD_ISSET(remote.sock, &write_sockets)) {
            Logger::trace("TCP-FORWARDER", "Event Loop: Write");
            if (write_handler(&remote)) {
              Logger::info("TCP-FORWARDER",
                           "Event Loop: Closing connection - %s:%d",
                           remote.partner_ip.data(), remote.partner_port);
              shutdown(remote.sock, SHUT_RDWR);
              close(remote.sock);
              to_delete.push_back(remote_pair.first);
            }
          }
        } else {
          Logger::error(
              "TCP-FORWARDER",
              "Event Loop: Socket should have already been deleted %s:%d!",
              remote.partner_ip.data(), remote.partner_port);
          to_delete.push_back(remote_pair.first);
        }
      }
      for (auto item : to_delete) {
        remotes.erase(item);
      }
      if (FD_ISSET(listen_sock, &read_sockets)) {
        listen_handler();
      }

      if (FD_ISSET(listen_sock, &write_sockets)) {
        Logger::info("TCP-FORWARDER", "Event Loop: Listen sock wants to write");
      }

      if (FD_ISSET(listen_sock, &error_sockets)) {
        Logger::fatal("TCP-FORWARDER", "Event Loop: Listen sock error");
      }
      if (FD_ISSET(signal_socket, &read_sockets)) {
        char buf[8];
        recv(signal_socket, buf, sizeof(buf), MSG_DONTWAIT);
      }
      if (FD_ISSET(signal_socket, &error_sockets)) {
        Logger::fatal("TCP-FORWARDER", "Event Loop: Signal socket error");
      }
    }
  }
}

void TCPForwarder::create_tcp_client(std::string_view peer_ip, uint32_t port) {
  int addr_family = AF_INET;
  int ip_protocol = IPPROTO_IP;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
  sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = inet_addr(peer_ip.data());
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(port);

  int sock_id = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (sock_id < 0) {
    Logger::error("TCP-FORWARDER", "Client socker create error - %d", errno);
    return;
  }
  int err = connect(sock_id, reinterpret_cast<sockaddr*>(&dest_addr),
                    sizeof(dest_addr));
  if (err != 0) {
    Logger::error("TCP-FORWARDER", "Client socket connect error - %s:%d - %d",
                  peer_ip.data(), port, errno);
    close(sock_id);
    return;
  }
  Logger::info("TCP-FORWARDER", "Client socket connected - %s:%d",
               peer_ip.data(), port);

  set_socket_options(sock_id);
  add_remote_connection(sock_id, std::string(peer_ip), port,
                        uActor::Remote::ConnectionRole::CLIENT);
}

void TCPForwarder::listen_handler() {
  int sock_id = 0;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
  struct sockaddr_in6 source_addr;
  uint addr_len = sizeof(source_addr);
  sock_id =
      accept(listen_sock, reinterpret_cast<sockaddr*>(&source_addr), &addr_len);

  if (sock_id < 0) {
    Logger::error("TCP-FORWARDER",
                  "Server unable to accept connection - error %d", errno);
    return;
  } else if (sock_id == 0) {
    Logger::error("TCP-FORWARDER", "Accept failed");
    return;
  }

  char addr_string[INET6_ADDRSTRLEN];
  uint16_t remote_port = 0;
  if (source_addr.sin6_family == PF_INET) {
    auto* addr = reinterpret_cast<sockaddr_in*>(&source_addr);
    inet_ntop(addr->sin_family, &addr->sin_addr, addr_string, INET_ADDRSTRLEN);
    remote_port = ntohs(addr->sin_port);
  } else if (source_addr.sin6_family == PF_INET6) {
    inet_ntop(source_addr.sin6_family, &source_addr.sin6_addr, addr_string,
              INET6_ADDRSTRLEN);
    remote_port = ntohs(source_addr.sin6_port);
  }
  Logger::info("TCP-FORWARDER", "Connection accepted - %s:%d", addr_string,
               remote_port);

  set_socket_options(sock_id);
  add_remote_connection(sock_id, std::string(addr_string), remote_port,
                        uActor::Remote::ConnectionRole::SERVER);
}

bool TCPForwarder::data_handler(uActor::Remote::TCPConnection* remote) {
  remote->last_read_contact = BoardFunctions::seconds_timestamp();
  remote->len =
      recv(remote->sock, remote->rx_buffer.data(), remote->rx_buffer.size(), 0);
  if (remote->len < 0) {
    Logger::error("TCP-FORWARDER", "Error occurred during receive - %d", errno);
    return true;
  } else if (remote->len == 0) {
    Logger::info("TCP-FORWARDER", "Connection closed - %s:%d",
                 remote->partner_ip.data(), remote->partner_port);
    return true;
  } else {
    remote->process_data(remote->len, remote->rx_buffer.data());
    return false;
  }
}

bool TCPForwarder::write_handler(uActor::Remote::TCPConnection* remote) {
#if defined(MSG_NOSIGNAL)  // POSIX
  int flag = MSG_NOSIGNAL | MSG_DONTWAIT;
#elif defined(SO_NOSIGPIPE)  // MacOS
  int flag = SO_NOSIGPIPE | MSG_DONTWAIT;
#endif
  while (!remote->write_buffer.empty()) {
    size_t remaining_size =
        remote->write_buffer.front()->size() - remote->write_offset;
    int written =
        send(remote->sock,
             remote->write_buffer.front()->data() + remote->write_offset,
             remaining_size, flag);
    if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      Logger::debug("TCP-FORWARDER", "Write error");
      return true;
    } else if (written == remaining_size) {
      Logger::trace("TCP-FORWARDER", "Write completed");
      remote->write_buffer.pop();
      remote->write_offset = 0;
    } else {
      Logger::trace("TCP-FORWARDER", "Write progress");
      if (written > 0) {
        remote->write_offset += written;
      }
      break;
    }
    remote->last_write_contact = BoardFunctions::seconds_timestamp();
  }
  return false;
}

void TCPForwarder::keepalive() {
  std::set<uint32_t> to_check;
  auto buffer = std::make_shared<std::vector<char>>(4, 0);

  std::unique_lock lock(remote_mtx);
  for (const auto& remote : remotes) {
    if (BoardFunctions::seconds_timestamp() - remote.second.last_read_contact >
            120 &&
        remote.second.last_read_contact > 0) {
      Logger::info(
          "TCP-FORWARDER", "Read check failed: %s - last contact: %d",
          remote.second.partner_node_id.c_str(),
          BoardFunctions::timestamp() - remote.second.last_read_contact);
      shutdown(remote.second.sock, SHUT_RDWR);
      continue;
    }

    if (BoardFunctions::seconds_timestamp() - remote.second.last_write_contact >
        10) {
      to_check.insert(remote.first);
    }
  }

  for (uint32_t remote_id : to_check) {
    auto remote_it = remotes.find(remote_id);
    if (remote_it == remotes.end()) {
      continue;
    }

    auto result = write(&remote_it->second, buffer, std::move(lock));
    lock = std::move(result.second);
    if (result.first) {
      Logger::info("TCP-FORWARDER", "Write check failed");
    } else {
      Logger::trace("TCP-FORWARDER", "Write check %s",
                    remote_it->second.partner_node_id.c_str());
      remote_it->second.last_write_contact =
          BoardFunctions::seconds_timestamp();
    }
  }
}

void TCPForwarder::add_remote_connection(int socket_id, std::string remote_addr,
                                         uint16_t remote_port,
                                         uActor::Remote::ConnectionRole role) {
  uint32_t remote_id = next_local_id++;
  if (auto [remote_it, inserted] =
          remotes.try_emplace(remote_id, remote_id, socket_id, remote_addr,
                              remote_port, role, this);
      inserted) {
    remote_it->second.last_read_contact = BoardFunctions::seconds_timestamp();
    if (role == uActor::Remote::ConnectionRole::CLIENT) {
      remote_it->second.send_routing_info();
    }
    signal_select_change();
    Logger::trace("TCP-FORWARDER", "Remote connection added");
  } else {
    Logger::warning("TCP-FORWARDER",
                    "Remote connection not inserted, closing "
                    "connection - "
                    "%s:%d!",
                    remote_addr.c_str(), remote_port);
    shutdown(socket_id, 0);
    close(socket_id);
  }
}

void TCPForwarder::signal_select_change() {
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-member-init, hicpp-member-init)
  sockaddr_in signal_dest_addr;
  signal_dest_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  signal_dest_addr.sin_family = AF_INET;
  signal_dest_addr.sin_port = htons(_address_arguments.port);
  sendto(signal_socket_write_handler, "A", strlen("A"), 0,
         (struct sockaddr*)&signal_dest_addr, sizeof(signal_dest_addr));
}

void TCPForwarder::set_socket_options(int socket_id) {
  int nodelay = 1;
  setsockopt(socket_id, IPPROTO_TCP, TCP_NODELAY, static_cast<void*>(&nodelay),
             sizeof(nodelay));

  // TODO(raphaelhetzel) Reevaluate the need for this
  // as we have an application-level keepalive system
  // uint32_t true_flag = 1;
  // uint32_t keepcnt = 3;
  // uint32_t keepidle = 60;
  // uint32_t keepintvl = 30;
  // setsockopt(socket_id, SOL_SOCKET, SO_KEEPALIVE, &true_flag,
  // sizeof(uint32_t)); setsockopt(socket_id, IPPROTO_TCP, TCP_KEEPCNT,
  // &keepcnt, sizeof(uint32_t)); setsockopt(socket_id, IPPROTO_TCP,
  // TCP_KEEPIDLE, &keepidle, sizeof(uint32_t)); setsockopt(socket_id,
  // IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl,
  //            sizeof(uint32_t));
}
}  // namespace uActor::Remote
