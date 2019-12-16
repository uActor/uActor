// TODO (raphaelhetzel) This is not cleaned up or completed yet,
// this is deferred until the new attribute based routing is done.

#ifndef MAIN_TCP_FORWARDER_HPP_
#define MAIN_TCP_FORWARDER_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
#include <lwip/netdb.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
}

#include <cmath>
#include <cstring>
#include <utility>
#include <algorithm>

#include "include/board_functions.hpp"
#include "include/router_v2.hpp"

static const char* TAG = "tcp_forwarder";

class TCPForwarder {
 public:
  static TCPForwarder& get_instance() {
    static TCPForwarder instance;
    return instance;
  }

  TCPForwarder() {
    RouterV2::getInstance().register_actor("core.forwarder.tcp/local/1");
    RouterV2::getInstance().register_alias("core.forwarder.tcp/local/1", "#");
  }

  static void os_task(void* args) {
    TCPForwarder fwd = TCPForwarder();
    xTaskCreatePinnedToCore(&receiver_task, "TCP-REC", 6168,
                            static_cast<void*>(&fwd), 4, nullptr, 1);
    while (true) {
      auto result = RouterV2::getInstance().receive(
          "core.forwarder.tcp/local/1", BoardFunctions::SLEEP_FOREVER);
      if (result) {
        fwd.receive(std::move(*result));
      }
    }
  }

  static void receiver_task(void* args) {
    TCPForwarder* fwd = reinterpret_cast<TCPForwarder*>(args);
    fwd->tcp_server();
  }

  void receive(Message&& m) {
    if (sock > 0) {
      int size = htonl(m.internal_size());
      if (write(sock, 4, reinterpret_cast<char*>(&size))) return;
      if (write(sock, m.internal_size(), m.full_buffer())) return;
    }
  }

 private:
  int sock = 0;

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
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(1337);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      vTaskDelete(NULL);
      return;
    }
    ESP_LOGI(TAG, "Socket created");

    int err =
        bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", 1337);

    err = listen(listen_sock, 1);
    if (err != 0) {
      ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
      goto CLEAN_UP;
    }

    while (1) {
      ESP_LOGI(TAG, "Socket listening");

      struct sockaddr_in6 source_addr;  // Large enough for both IPv4 or IPv6
      uint addr_len = sizeof(source_addr);
      sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
      if (sock < 0) {
        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        break;
      }

      // Convert ip address to string
      if (source_addr.sin6_family == PF_INET) {
        inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr.s_addr,
                    addr_str, sizeof(addr_str) - 1);
      } else if (source_addr.sin6_family == PF_INET6) {
        inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
      }
      ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

      char rx_buffer[1024];
      int len = 0;
      char* buffer = nullptr;
      int offset = 0;
      int size_remaining = 0;
      do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
        if (len < 0) {
          ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
          ESP_LOGW(TAG, "Connection closed");
        } else {
          ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
          if (buffer == nullptr) {
            if (len > 4) {
              size_remaining = ntohl(*reinterpret_cast<size_t*>(rx_buffer));
              buffer = new char[size_remaining];
              std::memcpy(buffer, rx_buffer + 4,
                          std::min(len - 4, size_remaining));
              offset += std::min(len - 4, size_remaining);
              size_remaining -= std::min(len - 4, size_remaining);
            }
          // TODO(raphaelhetzel) this part is untested && poentially incomplete
          } else if (buffer) {
            std::memcpy(buffer, rx_buffer, std::min(len, size_remaining));
            offset += std::min(len - 4, size_remaining);
            size_remaining -= std::min(len - 4, size_remaining);
          }
          if (size_remaining == 0) {
            printf("complete\n");
            Message m = Message("actor/local/45", "actor/local/2", 1237, 1500);
            std::memcpy(m.full_buffer(), buffer, 50);
            RouterV2::getInstance().send(std::move(m));

            delete buffer;
            buffer = nullptr;
          }
        }
      } while (len > 0);

      shutdown(sock, 0);
      close(sock);
      sock = 0;
    }

  CLEAN_UP:
    printf("clean up called\n");
    close(sock);
    sock = 0;
  }
};

#endif  //  MAIN_TCP_FORWARDER_HPP_
