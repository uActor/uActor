#ifndef MAIN_WIFI_STACK_HPP_
#define MAIN_WIFI_STACK_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

extern "C" {
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <lwip/err.h>
#include <lwip/sys.h>
}
#include <cstring>

#include "include/board_functions.hpp"

class WifiStack {
 public:
  static const int WIFI_CONNECTED_BIT = BIT0;
  static const int MAXIMUM_RETRIES = 5;
  constexpr static const char* TAG = "wifi stack";
  wifi_config_t wifi_config;

  WifiStack() {
    s_wifi_event_group = xEventGroupCreate();
    strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), CONFIG_WIFI_SSID,
            sizeof(wifi_config.sta.ssid));
    strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
            CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
  }

  void init(void) {
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t default_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&default_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler_wrapper, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler_wrapper, this));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
  }

  static void os_task(void* args) {
    WifiStack::get_instance().init();
    vTaskDelete(NULL);
  }

  static WifiStack& get_instance() {
    static WifiStack instance;
    return instance;
  }

 private:
  EventGroupHandle_t s_wifi_event_group;
  int retry_count = 0;

  void event_handler(esp_event_base_t event_base, int32_t event_id,
                     void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
      ESP_LOGI(TAG, "connect");
      esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (retry_count < MAXIMUM_RETRIES) {
        ESP_LOGI(TAG, "retry to connect to the AP");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        retry_count++;
      }
      ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event =
          reinterpret_cast<ip_event_got_ip_t*>(event_data);
      ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      retry_count = 0;
      xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
  }

  static void event_handler_wrapper(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    WifiStack* stack = reinterpret_cast<WifiStack*>(arg);
    stack->event_handler(event_base, event_id, event_data);
  }
};

#endif  // MAIN_WIFI_STACK_HPP_
