#include "remote/wifi_stack.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#include <support/testbed.h>

#include <string_view>
#include <utility>

extern "C" {
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <esp_wpa2.h>
#include <lwip/err.h>
#include <lwip/sys.h>
}

#include <cstring>

#include "pubsub/publication.hpp"
#include "pubsub/router.hpp"

namespace uActor::ESP32::Remote {

WifiStack::WifiStack() {
  s_wifi_event_group = xEventGroupCreate();
  strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), CONFIG_WIFI_SSID,
          sizeof(wifi_config.sta.ssid));
#if !(CONFIG_WIFI_USE_EDUROAM)
  strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
          CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
#endif
}

void WifiStack::sntp_synced(timeval* tv) {
  testbed_log_rt_integer("syncronized", 1);
}

void WifiStack::init(void) {
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
#if CONFIG_BENCHMARK_ENABLED
  sntp_setservername(0, "192.168.50.2");
#else
  sntp_setservername(0, "ntp1.lrz.de");
  sntp_setservername(1, "ntp3.lrz.de");
#endif
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  sntp_set_time_sync_notification_cb(&sntp_synced);
  sntp_init();

  ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void WifiStack::os_task(void* args) {
  WifiStack::get_instance().init();
  vTaskDelete(NULL);
}

void WifiStack::event_handler(esp_event_base_t event_base, int32_t event_id,
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
    ip_event_got_ip_t* event = reinterpret_cast<ip_event_got_ip_t*>(event_data);
    //  ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    testbed_log_ipv4_address(event->ip_info.ip);
    testbed_log_ipv4_netmask(event->ip_info.netmask);
    testbed_log_ipv4_gateway(event->ip_info.gw);

    // TODO(raphaelhetzel) this might arrive to early
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", IP2STR(&event->ip_info.ip));
    uActor::PubSub::Publication ip_info(uActor::BoardFunctions::NODE_ID, "root",
                                        "1");
    ip_info.set_attr("type", "notification");
    ip_info.set_attr("notification_text",
                     std::string("Current IP:\n") + buffer);
    ip_info.set_attr("notification_id", "ip");
    ip_info.set_attr("notification_lifetime", 0);
    uActor::PubSub::Router::get_instance().publish(std::move(ip_info));

    retry_count = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void WifiStack::event_handler_wrapper(void* arg, esp_event_base_t event_base,
                                      int32_t event_id, void* event_data) {
  WifiStack* stack = reinterpret_cast<WifiStack*>(arg);
  stack->event_handler(event_base, event_id, event_data);
}

}  // namespace uActor::ESP32::Remote
