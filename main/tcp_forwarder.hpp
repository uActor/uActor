// Basic socket handling (e.g. listen, retransmit) adapted from
// https://github.com/espressif/esp-idf/blob/a49b934ef895690f2b5e3709340db856e27475e2/examples/protocols/sockets/tcp_server/main/tcp_server.c
#ifndef MAIN_TCP_FORWARDER_HPP_
#define MAIN_TCP_FORWARDER_HPP_

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
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "include/board_functions.hpp"
#include "include/publication.hpp"
#include "include/remote_connection.hpp"
#include "include/subscription.hpp"

class TCPForwarder : public ForwarderSubscriptionAPI {
 public:
  constexpr static const char* TAG = "tcp_forwarder";

  TCPForwarder() : handle(PubSub::Router::get_instance().new_subscriber()) {
    PubSub::Filter primary_filter{
        PubSub::Constraint(std::string("node_id"), "node_1"),
        PubSub::Constraint(std::string("actor_type"), "forwarder"),
        PubSub::Constraint(std::string("instance_id"), "1")};
    forwarder_subscription_id = handle.subscribe(primary_filter);
  }

  static TCPForwarder& get_instance() {
    static TCPForwarder instance;
    return instance;
  }

  static void os_task(void* args) {
    TCPForwarder fwd = TCPForwarder();
    xTaskCreatePinnedToCore(&server_task, "TCP-SERVER", 6168,
                            static_cast<void*>(&fwd), 4, nullptr, 1);
    while (true) {
      auto result = fwd.handle.receive(BoardFunctions::SLEEP_FOREVER);
      if (result) {
        fwd.receive(std::move(*result));
      }
    }
  }

  static void server_task(void* args) {
    TCPForwarder* fwd = reinterpret_cast<TCPForwarder*>(args);
    fwd->tcp_server();
  }

  void receive(PubSub::MatchedPublication&& m) {
    if (m.subscription_id == forwarder_subscription_id) {
      ESP_LOGW(TAG, "Forwarder received unhandled message!");
      return;
    }

    auto receivers = subscription_mapping.find(m.subscription_id);
    if (receivers != subscription_mapping.end()) {
      for (const uint32_t subscriber_id : receivers->second) {
        auto& remote = remotes.at(subscriber_id);
        if (remote.sock > 0) {
          if (!(m.publication.has_attr("_internal_forwarded_by") &&
                std::get<std::string_view>(m.publication.get_attr(
                    "_internal_forwarded_by")) == remote.partner_node_id)) {
            std::string serialized = m.publication.to_msg_pack();
            int size = htonl(serialized.size());
            if (write(remote.sock, 4, reinterpret_cast<char*>(&size))) return;
            write(remote.sock, m.publication.to_msg_pack().size(),
                  serialized.c_str());
          }
        }
      }
    }
  }

  uint32_t add_subscription(uint32_t local_id, PubSub::Filter&& filter) {
    uint32_t sub_id = handle.subscribe(std::move(filter));
    auto entry = subscription_mapping.find(sub_id);
    if (entry != subscription_mapping.end()) {
      entry->second.insert(local_id);
    } else {
      subscription_mapping.emplace(sub_id, std::set<uint32_t>{local_id});
    }
    return sub_id;
  }

  void remove_subscription(uint32_t local_id, uint32_t sub_id) {
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
  PubSub::SubscriptionHandle handle;

  uint32_t next_local_id = 0;
  std::map<uint32_t, RemoteConnection> remotes;
  std::map<uint32_t, std::set<uint32_t>> subscription_mapping;

  int listen_sock;

  bool write(const int sock, const int len, const char* message) {
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

  void tcp_server() {
    fd_set rset;

    int addr_family;
    int ip_protocol;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(1337);

    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    char addr_str[128];
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

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
    ESP_LOGI(TAG, "Socket bound - listen address: %s", addr_str);

    err = listen(listen_sock, 1);
    if (err != 0) {
      ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
      close(listen_sock);
      listen_sock = 0;
    }

    while (1) {
      FD_ZERO(&rset);
      FD_SET(listen_sock, &rset);
      for (const auto& remote_pair : remotes) {
        FD_SET(remote_pair.second.sock, &rset);
      }
      int num_ready = select(FD_SETSIZE, &rset, NULL, NULL, NULL);
      for (const auto& remote_pair : remotes) {
        RemoteConnection& remote =
            const_cast<RemoteConnection&>(remote_pair.second);
        if (FD_ISSET(remote.sock, &rset)) {
          connection_handler(&remote);
        }
      }
      if (FD_ISSET(listen_sock, &rset)) {
        listen_handler();
      }
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
    remotes.try_emplace(remote_id, remote_id, this).first->second;
    RemoteConnection& remote = remotes.at(remote_id);
    remote.sock = sock_id;

    char addr_str[128];
    if (source_addr.sin6_family == PF_INET) {
      inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr.s_addr,
                  addr_str, sizeof(addr_str) - 1);
    } else if (source_addr.sin6_family == PF_INET6) {
      inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
    }
    ESP_LOGI(TAG, "Connection accepted - remote IP: %s", addr_str);
  }

  void connection_handler(RemoteConnection* remote) {
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

#endif  //  MAIN_TCP_FORWARDER_HPP_
