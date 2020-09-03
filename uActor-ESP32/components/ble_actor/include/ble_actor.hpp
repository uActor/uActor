#ifndef UACTOR_ESP32_COMPONENTS_BLE_ACTOR_INCLUDE_BLE_ACTOR_HPP_
#define UACTOR_ESP32_COMPONENTS_BLE_ACTOR_INCLUDE_BLE_ACTOR_HPP_

// BLE
#include <esp_nimble_hci.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
extern "C" {
#include <host/ble_hs.h>
}
#include <nimble/ble.h>
#include <services/gap/ble_svc_gap.h>
// There is a conflict with the C++ stdlib for min/max
#undef max
#undef min

#include <map>
#include <string>

#include "pubsub/receiver_handle.hpp"

namespace uActor::ESP32::BLE {
class BLEActor {
 public:
  static void os_task(void* args);

  BLEActor();

  ~BLEActor();

 private:
  PubSub::ReceiverHandle handle;

  void ble_init();

  void receive(PubSub::Publication&& publication);

  void handle_advertisement_receive(PubSub::Publication&& publication);

  static void host_task(void* param);

  static void on_reset(int reason);

  static void on_sync(void);

  static void set_random_addr();

  static void advertise(std::map<uint8_t, std::string>&& attributes);

  static void scan(void);

  static int discovery_callback(ble_gap_event* event, void* arg);

  static std::optional<std::string> serialize_attribute(uint8_t key,
                                                        std::string attribute);

  static void handle_discovery_event(ble_gap_event* event);
};
}  // namespace uActor::ESP32::BLE

#endif  // UACTOR_ESP32_COMPONENTS_BLE_ACTOR_INCLUDE_BLE_ACTOR_HPP_
