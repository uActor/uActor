// Basic socket handling (e.g. listen, retransmit) adapted from
// https://github.com/espressif/esp-idf/blob/a49b934ef895690f2b5e3709340db856e27475e2/examples/protocols/sockets/tcp_server/main/tcp_server.c
// https://github.com/espressif/esp-idf/blob/204492bd7838d3687719473a7de30876f3d1ee7e/examples/protocols/sockets/tcp_client/main/tcp_client.c
#ifndef MAIN_INCLUDE_ESP32_TCP_FORWARDER_HPP_
#define MAIN_INCLUDE_ESP32_TCP_FORWARDER_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "board_functions.hpp"
#include "pubsub/router.hpp"
#include "remote_connection.hpp"

class TCPForwarder : public ForwarderSubscriptionAPI {
 public:
  constexpr static const char* TAG = "tcp_forwarder";

  TCPForwarder()
      : handle(uActor::PubSub::Router::get_instance().new_subscriber()) {
    uActor::PubSub::Filter primary_filter{
        uActor::PubSub::Constraint(std::string("node_id"),
                                   BoardFunctions::NODE_ID),
        uActor::PubSub::Constraint(std::string("actor_type"), "forwarder"),
        uActor::PubSub::Constraint(std::string("instance_id"), "1")};
    forwarder_subscription_id = handle.subscribe(primary_filter);

    uActor::PubSub::Filter peer_announcements{
        uActor::PubSub::Constraint(std::string("type"), "tcp_client_connect"),
        uActor::PubSub::Constraint(std::string("node_id"),
                                   BoardFunctions::NODE_ID),
    };
    peer_announcement_subscription_id = handle.subscribe(peer_announcements);

    uActor::PubSub::Filter subscription_update{
        uActor::PubSub::Constraint(std::string("type"),
                                   "local_subscription_update"),
        uActor::PubSub::Constraint(std::string("publisher_node_id"),
                                   BoardFunctions::NODE_ID),
    };
    subscription_update_subscription_id = handle.subscribe(subscription_update);
  }

  static TCPForwarder& get_instance() {
    static TCPForwarder instance;
    return instance;
  }

  static void os_task(void* args) {
    TCPForwarder fwd = TCPForwarder();
    xTaskCreatePinnedToCore(&tcp_reader_task, "TCP-SERVER", 5000,
                            static_cast<void*>(&fwd), 4, nullptr, 0);
    while (true) {
      auto result = fwd.handle.receive(BoardFunctions::SLEEP_FOREVER);
      if (result) {
        fwd.receive(std::move(*result));
      }
    }
  }

  static void tcp_reader_task(void* args) {
    TCPForwarder* fwd = reinterpret_cast<TCPForwarder*>(args);
    fwd->tcp_reader();
  }

  void receive(uActor::PubSub::MatchedPublication&& m) {
    std::unique_lock remote_lock(remote_mtx);

    if (m.subscription_id == forwarder_subscription_id) {
      ESP_LOGW(TAG, "Forwarder received unhandled message!");
      return;
    }

    if (m.subscription_id == subscription_update_subscription_id) {
      if (m.publication.get_str_attr("type") == "local_subscription_update") {
        for (auto& receiver : remotes) {
          receiver.second.handle_subscription_update_notification(
              m.publication);
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
              } else if (sequence_info_it->second.is_older_than(
                             *sequence_number, *epoch_number)) {
                sequence_info_it->second = std::move(new_sequence_info);
              } else {
                skip = true;
              }
            }
          }

          if (!skip) {
            if (write(remote.sock, 4, reinterpret_cast<char*>(&size)) ||
                write(remote.sock, serialized.size(), serialized.c_str())) {
              remotes.erase(remote_it);
              continue;
            }
          }
        }
      }
    }
  }

  uint32_t add_subscription(uint32_t local_id, uActor::PubSub::Filter&& filter,
                            std::string_view node_id) {
    uint32_t sub_id = handle.subscribe(std::move(filter), node_id);
    auto entry = subscription_mapping.find(sub_id);
    if (entry != subscription_mapping.end()) {
      entry->second.insert(local_id);
    } else {
      subscription_mapping.emplace(sub_id, std::set<uint32_t>{local_id});
    }
    return sub_id;
  }

  void remove_subscription(uint32_t local_id, uint32_t sub_id,
                           std::string_view node_id) {
    auto it = subscription_mapping.find(sub_id);
    if (it != subscription_mapping.end()) {
      it->second.erase(local_id);
      if (it->second.empty()) {
        handle.unsubscribe(sub_id);
        subscription_mapping.erase(it);
      }
    }
  }

 private:
  int forwarder_subscription_id;
  int peer_announcement_subscription_id;
  int subscription_update_subscription_id;
  uActor::PubSub::ReceiverHandle handle;

  uint32_t next_local_id = 0;
  std::map<uint32_t, RemoteConnection> remotes;
  std::map<uint32_t, std::set<uint32_t>> subscription_mapping;

  std::mutex remote_mtx;
  int listen_sock;

  bool write(int sock, int len, const char* message) {
    int to_write = len;
    while (to_write > 0) {
      int written = send(sock, message + (len - to_write), to_write, 0);
      if (written < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        return true;
      }
      to_write -= written;
    }
    return false;
  }

  void tcp_reader() {
    fd_set sockets_to_read;

    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(1337);

    listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      vTaskDelete(NULL);
      return;
    }

    int err =
        bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      close(listen_sock);
      listen_sock = 0;
    }

    char addr_str[128];
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
    ESP_LOGI(TAG, "Socket bound - listen address: %s", addr_str);

    err = listen(listen_sock, 1);
    if (err != 0) {
      ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
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
          select(max_val + 1, &sockets_to_read, NULL, NULL, &timeout);
      remote_lock.lock();

      if (num_ready > 0) {
        for (const auto& remote_pair : remotes) {
          RemoteConnection& remote =
              const_cast<RemoteConnection&>(remote_pair.second);
          if (FD_ISSET(remote.sock, &sockets_to_read)) {
            data_handler(&remote);
          }
        }
        if (FD_ISSET(listen_sock, &sockets_to_read)) {
          listen_handler();
        }
      }
    }
  }

  void create_tcp_client(std::string_view peer_ip, uint32_t port) {
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(peer_ip.data());
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    char addr_str[128];
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

    int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      return;
    }
    ESP_LOGI(TAG, "Socket created, connecting to %s:%d", peer_ip.data(),
             ntohs(dest_addr.sin_port));

    int err = connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
      return;
    }
    ESP_LOGI(TAG, "Successfully connected");

    uint32_t remote_id = next_local_id++;
    if (auto [remote_it, inserted] =
            remotes.try_emplace(remote_id, remote_id, sock, this);
        inserted) {
      remote_it->second.send_routing_info();
      int nodelay = 1;
      setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, static_cast<void*>(&nodelay),
                 sizeof(nodelay));
    }
  }

  void listen_handler() {
    int sock_id = 0;

    struct sockaddr_in6 source_addr;
    uint addr_len = sizeof(source_addr);
    sock_id = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);

    if (sock_id < 0) {
      ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
      return;
    }

    uint32_t remote_id = next_local_id++;
    if (auto [remote_it, inserted] =
            remotes.try_emplace(remote_id, remote_id, sock_id, this);
        inserted) {
      remote_it->second.send_routing_info();

      int nodelay = 1;
      setsockopt(sock_id, IPPROTO_TCP, TCP_NODELAY,
                 static_cast<void*>(&nodelay), sizeof(nodelay));
    }

    char addr_str[128];
    if (source_addr.sin6_family == PF_INET) {
      inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr.s_addr,
                  addr_str, sizeof(addr_str) - 1);
    } else if (source_addr.sin6_family == PF_INET6) {
      inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
    }
    ESP_LOGI(TAG, "Connection accepted - remote IP: %s", addr_str);
  }

  void data_handler(RemoteConnection* remote) {
    remote->len = recv(remote->sock, remote->rx_buffer.data(),
                       remote->rx_buffer.size(), 0);
    if (remote->len < 0) {
      ESP_LOGE(TAG, "Error occurred during receive: %d", errno);
    } else if (remote->len == 0) {
      ESP_LOGI(TAG, "Connection closed");
    } else {
      remote->process_data(remote->len, remote->rx_buffer.data());
      return;
    }

    shutdown(remote->sock, 0);
    close(remote->sock);
    remotes.erase(remote->local_id);
  }
};

#endif  //  MAIN_INCLUDE_ESP32_TCP_FORWARDER_HPP_
