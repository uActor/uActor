// Adapted from
// https://github.com/espressif/esp-idf/blob/204492bd7838d3687719473a7de30876f3d1ee7e/examples/wifi/getting_started/station/main/station_example_main.c

#ifndef MAIN_INCLUDE_ESP32_REMOTE_WIFI_STACK_HPP_
#define MAIN_INCLUDE_ESP32_REMOTE_WIFI_STACK_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

extern "C" {
#include <esp_wifi.h>
}
namespace uActor::ESP32::Remote {

class WifiStack {
 public:
  static const int WIFI_CONNECTED_BIT = BIT0;
  static const int MAXIMUM_RETRIES = 5;
  constexpr static const char* TAG = "wifi stack";

  static WifiStack& get_instance() {
    static WifiStack instance;
    return instance;
  }

  static void os_task(void* args);

  WifiStack();

  static void sntp_synced(timeval* tv);

  void init(void);

 private:
  EventGroupHandle_t s_wifi_event_group;
  wifi_config_t wifi_config;
  int retry_count = 0;

  void event_handler(esp_event_base_t event_base, int32_t event_id,
                     void* event_data);

  static void event_handler_wrapper(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data);
};

}  // namespace uActor::ESP32::Remote

#endif  // MAIN_INCLUDE_ESP32_REMOTE_WIFI_STACK_HPP_
