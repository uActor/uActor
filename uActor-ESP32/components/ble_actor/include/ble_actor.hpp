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

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <variant>

#include "pubsub/filter.hpp"
#include "pubsub/receiver_handle.hpp"

namespace uActor::ESP32::BLE {

template <typename Visitor>
static void walk_ble_packet(const uint8_t* data, size_t data_size,
                            Visitor visitor) {
  uint8_t current_attribute_size = 0;
  uint8_t current_attribute_type = 0;
  const uint8_t* current_data_start = nullptr;

  for (const uint8_t* attribute_start = data;
       attribute_start < (data + data_size);
       attribute_start = attribute_start + current_attribute_size + 1) {
    current_attribute_size = *attribute_start;
    if (current_attribute_size == 0) {
      break;
    }
    current_attribute_type = *(attribute_start + 1);
    current_data_start = attribute_start + 2;

    visitor(current_attribute_type,
            std::string(reinterpret_cast<const char*>(current_data_start),
                        current_attribute_size - 1));
  }
}

class BLEActor {
 public:
#if CONFIG_UACTOR_OPTIMIZATIONS_BLE_FILTER
  class InternalBLEFilter {
   public:
    void set_field(uint8_t field, std::string&& data,
                   PubSub::ConstraintPredicates::Predicate predicate) {
      // TODO(raphaelhetzel) if required, implement
      if (predicate == PubSub::ConstraintPredicates::Predicate::EQ) {
        if (_fields.find(field) == _fields.end()) {
          count_fields++;
        }
        if (data.empty()) {
          count_fields--;
        } else {
          _fields.try_emplace(field, std::move(data));
          match_all = false;
        }
      }
    }

    void filter(PubSub::Filter&& filter) { _filter = std::move(filter); }

    const PubSub::Filter& filter() const { return _filter; }

    bool sub_matches(const uint8_t* data, size_t data_size) const {
      if (match_all) {
        return true;
      }

      bool match = true;
      size_t num_matches = 0;
      walk_ble_packet(
          data, data_size,
          [&fields = _fields, &match = match, &num_matches = num_matches](
              uint8_t attribute_id, const std::string& value) {
            auto field_it = fields.find(attribute_id);
            if (field_it != fields.end()) {
              if (field_it->second == value) {
                num_matches++;
              }
            }
          });
      return num_matches == count_fields;
    }

   private:
    std::map<uint8_t, std::string> _fields;
    size_t count_fields = 0;
    PubSub::Filter _filter;
    bool match_all = true;
  };
#endif

  static void os_task(void* args);

  BLEActor();

  ~BLEActor();

 private:
  PubSub::ReceiverHandle handle;
  PubSub::ActorIdentifier own_identifier;

#if CONFIG_UACTOR_OPTIMIZATIONS_BLE_FILTER
  static std::list<InternalBLEFilter> active_filters;
#endif

  void ble_init();

  void receive(PubSub::Publication&& publication);

  void handle_advertisement_receive(PubSub::Publication&& publication);

#if CONFIG_UACTOR_OPTIMIZATIONS_BLE_FILTER
  void handle_subscription_added(PubSub::Publication&& publication);
  void handle_subscription_removed(PubSub::Publication&& publication);
#endif

  static bool filter_has_ble_type_constraint(const PubSub::Filter& filter);

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
