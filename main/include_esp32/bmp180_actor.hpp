#ifndef MAIN_INCLUDE_ESP32_BMP180_ACTOR_HPP_
#define MAIN_INCLUDE_ESP32_BMP180_ACTOR_HPP_

#include <bmp180.h>

#include <array>
#include <string>
#include <utility>

#include "native_actor.hpp"

class BMP180Actor : public NativeActor {
 public:
  BMP180Actor(ManagedNativeActor* actor_wrapper, std::string_view node_id,
              std::string_view actor_type, std::string_view instance_id)
      : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {
    bmp180_init(21, 22);

    uActor::PubSub::Publication register_temperature;
    register_temperature.set_attr("type", "unmanaged_actor_update");
    register_temperature.set_attr("command", "register");
    register_temperature.set_attr("update_actor_type",
                                  "core.sensors.temperature");
    register_temperature.set_attr("node_id", node_id.data());
    publish(std::move(register_temperature));

    uActor::PubSub::Publication register_pressure;
    register_pressure.set_attr("type", "unmanaged_actor_update");
    register_pressure.set_attr("command", "register");
    register_pressure.set_attr("update_actor_type", "core.sensors.pressure");
    register_pressure.set_attr("node_id", node_id.data());
    publish(std::move(register_pressure));
  }

  ~BMP180Actor() {
    uActor::PubSub::Publication deregister_temperature;
    deregister_temperature.set_attr("type", "unmanaged_actor_update");
    deregister_temperature.set_attr("command", "register");
    deregister_temperature.set_attr("update_actor_type",
                                    "core.sensors.temperature");
    deregister_temperature.set_attr("node_id", node_id().data());
    publish(std::move(deregister_temperature));

    uActor::PubSub::Publication deregister_pressure;
    deregister_pressure.set_attr("type", "unmanaged_actor_update");
    deregister_pressure.set_attr("command", "register");
    deregister_pressure.set_attr("update_actor_type", "core.sensors.pressure");
    deregister_pressure.set_attr("node_id", node_id().data());
    publish(std::move(deregister_pressure));
  }

  void receive(const uActor::PubSub::Publication& publication) {
    if (publication.get_str_attr("type") == "init" ||
        publication.get_str_attr("type") == "trigger_update") {
      send_temperature_update();
      send_pressure_update();
      send_delayed_trigger(1000);
    }
  }

 private:
  void send_temperature_update() {
    float temperature;
    if (!bmp180_read_temperature(&temperature)) {
      uActor::PubSub::Publication temperature_update;
      temperature_update.set_attr("type", "temperature_update");
      temperature_update.set_attr("temperature", temperature);
      temperature_update.set_attr("node_id", node_id().data());
      publish(std::move(temperature_update));
    }
  }

  void send_pressure_update() {
    uint32_t pressure;
    if (!bmp180_read_pressure(&pressure)) {
      uActor::PubSub::Publication pressure_update;
      pressure_update.set_attr("type", "pressure_update");
      pressure_update.set_attr("pressure", static_cast<int32_t>(pressure));
      pressure_update.set_attr("node_id", node_id().data());
      publish(std::move(pressure_update));
    }
  }

  void send_delayed_trigger(uint32_t delay) {
    uActor::PubSub::Publication trigger;
    trigger.set_attr("type", "trigger_update");
    trigger.set_attr("node_id", node_id().data());
    trigger.set_attr("actor_type", actor_type().data());
    trigger.set_attr("instance_id", instance_id().data());
    delayed_publish(std::move(trigger), delay);
  }
};

#endif  //  MAIN_INCLUDE_ESP32_BMP180_ACTOR_HPP_
