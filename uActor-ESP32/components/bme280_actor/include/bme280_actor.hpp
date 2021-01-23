#ifndef UACTOR_ESP32_COMPONENTS_BME280_ACTOR_INCLUDE_BME280_ACTOR_HPP_
#define UACTOR_ESP32_COMPONENTS_BME280_ACTOR_INCLUDE_BME280_ACTOR_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
extern "C" {
#include <esp_system.h>
}
#include <bme280.h>
#include <driver/i2c.h>

#include <set>
#include <string>
#include <string_view>

#include "actor_runtime/native_actor.hpp"
#include "support/logger.hpp"

namespace uActor::ESP32::Sensors {

class BME280Actor : public ActorRuntime::NativeActor {
 public:
  BME280Actor(ActorRuntime::ManagedNativeActor *actor_wrapper,
              std::string_view node_id, std::string_view actor_type,
              std::string_view instance_id);

  ~BME280Actor() override;

  void receive(const PubSub::Publication &publication) override;

 private:
  uint32_t ready_retries = 0;
  std::set<std::string> registered_actors;

  bme280_dev dev;
  int8_t rslt = BME280_OK;
  uint8_t dev_addr = BME280_I2C_ADDR_PRIM;

  bool calibrated_temperature = false;
  bool calibrated_pressure = false;
  bool calibrated_humidity = false;

  void fetch_send_updates();

  void send_update(std::string variable, std::string unit, float value,
                   bool calibrated);
  void send_failure_notification();
  void send_delayed_trigger(uint32_t delay, std::string type);
  void send_exit_message();

  void register_unmanaged_actor(std::string type);
  void deregister_managed_actor(std::string type);

  static void user_delay_us(uint32_t period, void *intf_ptr);

  static int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len,
                              void *intf_ptr);

  static int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
                               uint32_t len, void *intf_ptr);
};

}  // namespace uActor::ESP32::Sensors

#endif  //  UACTOR_ESP32_COMPONENTS_BME280_ACTOR_INCLUDE_BME280_ACTOR_HPP_
