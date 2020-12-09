#include "scd30_actor.hpp"


#include <string_view>
#include <utility>
#include <set>

namespace uActor::ESP32::Sensors {

  SCD30Actor::SCD30Actor(ActorRuntime::ManagedNativeActor* actor_wrapper,
              std::string_view node_id, std::string_view actor_type,
              std::string_view instance_id)
      : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {}

  SCD30Actor::~SCD30Actor() {
    for(const auto& actor_type : registered_actors) {
      deregister_managed_actor(actor_type);
    }
    registered_actors.clear();
  }

  void SCD30Actor::receive(const PubSub::Publication& publication) {
    if (publication.get_str_attr("type") == "init" || publication.get_str_attr("type") == "trigger_sensor_wait"){
      if(scd30_probe() != STATUS_OK) {
        Support::Logger::warning("SCD30", "INIT", "Sensor not found\n");
        if(ready_retries < 5) {
          send_delayed_trigger(1000, "trigger_sensor_wait");
        } else {
          send_exit_message();
        }
        ready_retries++;
      } else {
        scd30_start_periodic_measurement(960);
        register_unmanaged_actor("core.sensors.temperature");
        register_unmanaged_actor("core.sensors.relative_humidity");
        register_unmanaged_actor("core.sensors.co2");
        send_delayed_trigger(10100, "trigger_measurement");
      }
    } else if(publication.get_str_attr("type") == "exit") {
      for(auto actor_type : registered_actors) {
        deregister_managed_actor(actor_type);
      }
      registered_actors.clear();
    } else if(publication.get_str_attr("type") == "trigger_measurement") {
      fetch_send_updates();
    }
  }
  
  void SCD30Actor::fetch_send_updates() {
    float co2, temperature, relative_humidity;
    uint16_t ready = false;
    uint32_t counter = 0;
    while(!ready && counter < 5) {
      if(scd30_get_data_ready(&ready)) {
        Support::Logger::warning("SCD30", "FETCH", "Ready error\n");
        send_failure_notification();
      }
      counter++;
    }
    if(!ready) {
      Support::Logger::info("SCD30", "FETCH", "Sensor not ready, waiting 100ms\n");
      send_delayed_trigger(100, "trigger_measurement");
      return;
    }
    if (scd30_read_measurement(&co2, &temperature, &relative_humidity) != STATUS_OK) {
      Support::Logger::warning("SCD30", "FETCH", "Read error\n");
      send_failure_notification();
      send_delayed_trigger(10000, "trigger_measurement");
    } else {
      Support::Logger::trace("SCD30", "FETCH", "read: %f %f %f\n", temperature, relative_humidity, co2);
      send_update("temperature", "degree_celsius", temperature);
      send_update("co2", "ppm", co2);
      send_update("relative_humidity", "percent", relative_humidity);
      send_delayed_trigger(10000, "trigger_measurement");
    }
  }

  void SCD30Actor::send_update(std::string variable, std::string unit, float value) {
    PubSub::Publication sensor_update;
    sensor_update.set_attr("type", std::string("sensor_update_"+variable));
    sensor_update.set_attr("value", value);
    sensor_update.set_attr("sensor", "scd30");
    sensor_update.set_attr("unit", unit);
    publish(std::move(sensor_update));
  }

  void SCD30Actor::send_failure_notification() {
    PubSub::Publication sensor_failure;
    sensor_failure.set_attr("type", "sensor_failure");
    sensor_failure.set_attr("sensor", "scd30");
    publish(std::move(sensor_failure));
  }

  void SCD30Actor::send_delayed_trigger(uint32_t delay, std::string type) {
    PubSub::Publication trigger;
    trigger.set_attr("type", type);
    trigger.set_attr("node_id", node_id().data());
    trigger.set_attr("actor_type", actor_type().data());
    trigger.set_attr("instance_id", instance_id().data());
    delayed_publish(std::move(trigger), delay);
  }

  void SCD30Actor::send_exit_message() {
    PubSub::Publication exit_message;
    exit_message.set_attr("node_id", node_id());
    exit_message.set_attr("actor_type", actor_type());
    exit_message.set_attr("instance_id", instance_id());
    exit_message.set_attr("type", "exit");
    publish(std::move(exit_message));
  }

  void SCD30Actor::register_unmanaged_actor(std::string type) {
    if(!registered_actors.insert(type).second) {
      return;
    }
    PubSub::Publication register_message;
    register_message.set_attr("type", "unmanaged_actor_update");
    register_message.set_attr("command", "register");
    register_message.set_attr("update_actor_type", std::move(type));
    register_message.set_attr("node_id", node_id());
    publish(std::move(register_message));
  }

  void SCD30Actor::deregister_managed_actor(std::string type) {
    PubSub::Publication deregister_message;
    deregister_message.set_attr("type", "unmanaged_actor_update");
    deregister_message.set_attr("command", "deregister");
    deregister_message.set_attr("update_actor_type", type);
    deregister_message.set_attr("node_id", node_id());
    publish(std::move(deregister_message)); 
  }

}  // namespace uActor::ESP32::Sensors