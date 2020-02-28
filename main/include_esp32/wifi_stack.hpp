// Adapted from
// https://github.com/espressif/esp-idf/blob/204492bd7838d3687719473a7de30876f3d1ee7e/examples/wifi/getting_started/station/main/station_example_main.c

#ifndef MAIN_INCLUDE_ESP32_WIFI_STACK_HPP_
#define MAIN_INCLUDE_ESP32_WIFI_STACK_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <testbed.h>

extern "C" {
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_wpa2.h>
#include <lwip/err.h>
#include <lwip/sys.h>
}
#include <cstring>

#include "board_functions.hpp"

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
#if !(CONFIG_WIFI_USE_EDUROAM)
    strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
            CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
#endif
  }

  static void sntp_synced(timeval* tv) {
    testbed_log_integer("syncronized", 1, true);
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

#if CONFIG_WIFI_USE_EDUROAM
    extern uint8_t ca_pem_start[] asm("_binary_eduroam_pem_start");
    extern uint8_t ca_pem_end[] asm("_binary_eduroam_pem_end");
    uint32_t ca_pem_bytes = ca_pem_end - ca_pem_start;
    ESP_ERROR_CHECK(
        esp_wifi_sta_wpa2_ent_set_ca_cert(ca_pem_start, ca_pem_bytes));
    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_username(
        reinterpret_cast<uint8_t*>(const_cast<char*>(CONFIG_WIFI_USERNAME)),
        strlen(CONFIG_WIFI_USERNAME)));
    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_password(
        reinterpret_cast<uint8_t*>(const_cast<char*>(CONFIG_WIFI_PASSWORD)),
        strlen(CONFIG_WIFI_PASSWORD)));
    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_enable());
#endif

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK(esp_wifi_start());

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp1.lrz.de");
    sntp_setservername(1, "ntp3.lrz.de");
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_time_sync_notification_cb(&sntp_synced);
    sntp_init();

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
      sntp_restart();
      ip_event_got_ip_t* event =
          reinterpret_cast<ip_event_got_ip_t*>(event_data);
      //  ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
      testbed_log_ipv4_address(event->ip_info.ip);
      testbed_log_ipv4_netmask(event->ip_info.netmask);
      testbed_log_ipv4_gateway(event->ip_info.gw);

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

#endif  // MAIN_INCLUDE_ESP32_WIFI_STACK_HPP_