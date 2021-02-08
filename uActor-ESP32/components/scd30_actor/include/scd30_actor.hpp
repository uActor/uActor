#ifndef UACTOR_ESP32_COMPONENTS_SCD30_ACTOR_INCLUDE_SCD30_ACTOR_HPP_
#define UACTOR_ESP32_COMPONENTS_SCD30_ACTOR_INCLUDE_SCD30_ACTOR_HPP_

#include <scd30.h>

#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "actor_runtime/native_actor.hpp"
#include "support/logger.hpp"

namespace uActor::ESP32::Sensors {

class SCD30Actor : public ActorRuntime::NativeActor {
 public:
  SCD30Actor(ActorRuntime::ManagedNativeActor* actor_wrapper,
             std::string_view node_id, std::string_view actor_type,
             std::string_view instance_id);

  ~SCD30Actor() override;

  void receive(const PubSub::Publication& publication) override;

 private:
  uint32_t ready_retries = 0;
  std::set<std::string> registered_actors;

  bool calibrated_temp = false;
  bool calibrated_co2 = false;
  uint16_t current_offset_temp = 0;

  void fetch_send_updates();
  void calibrate_temperature(float server_offset);
  void co2_forced_calibration(uint16_t ppm);
  uint32_t init_timestamp = 0;

  std::pair<bool, uint16_t> read_temperature_calibrarion_value();
  void write_temperature_calibration_value(uint16_t calibration_value);
  std::pair<bool, uint32_t> read_co2_configuration_timestamp();
  void write_co2_calibration_timestamp(uint32_t calibration_timestamp);

  void send_update(std::string variable, std::string unit, float value,
                   bool calibrated);
  void send_failure_notification();
  void send_exit_message();
  void fetch_calibration_data();

  void register_unmanaged_actor(std::string type);
  void deregister_managed_actor(std::string type);
  void update_calibration_notification(std::string_view notification_text);
};

}  // namespace uActor::ESP32::Sensors

#endif  //  UACTOR_ESP32_COMPONENTS_SCD30_ACTOR_INCLUDE_SCD30_ACTOR_HPP_
