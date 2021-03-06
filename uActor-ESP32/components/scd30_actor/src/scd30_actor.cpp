#include "scd30_actor.hpp"

#include <nvs.h>
#include <nvs_flash.h>

#include <array>
#include <ctime>
#include <set>
#include <string_view>
#include <utility>

namespace uActor::ESP32::Sensors {

SCD30Actor::SCD30Actor(ActorRuntime::ManagedNativeActor* actor_wrapper,
                       std::string_view node_id, std::string_view actor_type,
                       std::string_view instance_id)
    : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {}

SCD30Actor::~SCD30Actor() {
  for (const auto& actor_type : registered_actors) {
    deregister_managed_actor(actor_type);
  }
  registered_actors.clear();
}

void SCD30Actor::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "init") {
    {
      auto [is_temp_calibrated, temp_calibration_value] =
          read_temperature_calibrarion_value();
      if (is_temp_calibrated) {
        Support::Logger::info("SCD30-ACTOR",
                              "Temperature calibrated with offset: %d.",
                              temp_calibration_value);
        calibrated_temp = true;
        current_offset_temp = temp_calibration_value;
      } else {
        Support::Logger::info("SCD30-ACTOR", "Temperature not calibrated.\n");
        scd30_set_temperature_offset(0);
        fetch_calibration_data();
      }
    }
    {
      auto [is_co2_calibrated, co2_calibration_timestamp] =
          read_co2_configuration_timestamp();
      if (is_co2_calibrated) {
        Support::Logger::info("SCD30-ACTOR",
                              "CO2 sensor was last calibrated: %d.\n",
                              co2_calibration_timestamp);
        calibrated_co2 = true;
      } else {
        Support::Logger::info("SCD30-ACTOR",
                              "CO2 Sensor needs to be calibrated.");
        subscribe(PubSub::Filter{
            PubSub::Constraint("command", "initialize_outdoor_calibration"),
            PubSub::Constraint("sensor", "scd30_co2"),
            PubSub::Constraint("node_id", node_id())});
        // This is an alternative approach that automatically calibrates the
        // sensor after 6 minutes enqueue_wakeup(360000, "calibrate_co2");
        // update_calibration_notification("calibration\nscheduled");
      }
    }
  }
  if (publication.get_str_attr("type") == "init" ||
      (publication.get_str_attr("type") == "wakeup" &&
       publication.get_str_attr("wakeup_id") == "trigger_sensor_wait")) {
    if (scd30_probe() != STATUS_OK) {
      Support::Logger::warning("SCD30-ACTOR", "Sensor not found\n");
      if (ready_retries < 5) {
        enqueue_wakeup(1000, "trigger_sensor_wait");
      } else {
        send_exit_message();
      }
      ready_retries++;
    } else {
      scd30_start_periodic_measurement(955);
      register_unmanaged_actor("core.sensors.temperature");
      register_unmanaged_actor("core.sensors.relative_humidity");
      register_unmanaged_actor("core.sensors.co2");
      init_timestamp = BoardFunctions::seconds_timestamp();
      enqueue_wakeup(20100, "trigger_measurement");
    }
  } else if (publication.get_str_attr("type") == "exit") {
    for (auto actor_type : registered_actors) {
      deregister_managed_actor(actor_type);
    }
    registered_actors.clear();
  } else if (publication.get_str_attr("type") == "wakeup" &&
             publication.get_str_attr("wakeup_id") == "trigger_measurement") {
    fetch_send_updates();
    if (!calibrated_temp) {
      fetch_calibration_data();
    }
  } else if (publication.get_str_attr("command") == "static_calibration") {
    return;  // TODO(raphaelhetzel) This is disabled on purpose
    if (!calibrated_temp) {
      fetch_calibration_data();
    }
  } else if ((publication.get_str_attr("command") ==
                  "initialize_outdoor_calibration" &&
              publication.get_str_attr("sensor") == "scd30_co2") ||
             (publication.get_str_attr("type") == "wakeup" &&
              publication.get_str_attr("wakeup_id") == "calibrate_co2")) {
    uint16_t target_ppm = 400;
    if (publication.has_attr("target_ppm")) {
      target_ppm = *publication.get_int_attr("target_ppm");
      if (target_ppm < 400 || target_ppm > 2000) {
        return;
      }
    }
    co2_forced_calibration(target_ppm);
  } else if (publication.get_str_attr("type") == "sensor_calibration_data" &&
             publication.get_str_attr("sensor_type") == "scd30_temperature") {
    if (publication.has_attr("value")) {
      calibrate_temperature(*publication.get_float_attr("value"));
    }
  }
}

void SCD30Actor::fetch_send_updates() {
  float co2, temperature, relative_humidity;
  uint16_t ready = false;
  uint32_t counter = 0;
  while (!ready && counter < 5) {
    if (scd30_get_data_ready(&ready)) {
      Support::Logger::warning("SCD30-ACTOR", "Fetch: Ready error");
      send_failure_notification();
    }
    counter++;
  }
  if (!ready) {
    Support::Logger::info("SCD30-ACTOR",
                          "Fetch: Sensor not ready, waiting 100ms");
    enqueue_wakeup(100, "trigger_measurement");
    return;
  }
  if (scd30_read_measurement(&co2, &temperature, &relative_humidity) !=
      STATUS_OK) {
    Support::Logger::warning("SCD30-ACTOR", "Fetch: Read error");
    send_failure_notification();
    enqueue_wakeup(20000, "trigger_measurement");
  } else {
    send_update("temperature", "degree_celsius", temperature, calibrated_temp);
    send_update("co2", "ppm", co2, calibrated_co2);
    send_update("relative_humidity", "percent", relative_humidity,
                calibrated_temp);
    enqueue_wakeup(20000, "trigger_measurement");
  }
}

void SCD30Actor::send_update(std::string variable, std::string unit,
                             float value, bool calibrated) {
  PubSub::Publication sensor_update;
  sensor_update.set_attr("type", std::string("sensor_update_" + variable));
  sensor_update.set_attr("value", value);
  sensor_update.set_attr("sensor", "scd30");
  sensor_update.set_attr("unit", unit);
  int c = calibrated ? 1 : 0;
  sensor_update.set_attr("calibrated", c);
  publish(std::move(sensor_update));
}

void SCD30Actor::send_failure_notification() {
  PubSub::Publication sensor_failure;
  sensor_failure.set_attr("type", "sensor_failure");
  sensor_failure.set_attr("sensor", "scd30");
  publish(std::move(sensor_failure));
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
  if (!registered_actors.insert(type).second) {
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

void SCD30Actor::fetch_calibration_data() {
  Support::Logger::info("SCD30-ACTOR", "Fetch Calibration",
                        "Fetch calibration data\n");
  PubSub::Publication fetch_calibration;
  fetch_calibration.set_attr("command", "fetch_calibration_data");
  fetch_calibration.set_attr("c_node_id", node_id());
  fetch_calibration.set_attr("sensor_type", "scd30_temperature");
  publish(std::move(fetch_calibration));
}

void SCD30Actor::calibrate_temperature(float server_offset) {
  Support::Logger::info("SCD30-ACTOR", "Start calibration\n");

  uint16_t temperature_offset = static_cast<uint16_t>(server_offset * -100);

  if (calibrated_temp && temperature_offset == current_offset_temp) {
    return;
  }

  if (scd30_probe() != STATUS_OK) {
    Support::Logger::info("SCD30-ACTOR", "Calibration: Sensor not found");
    send_failure_notification();
  }

  if (scd30_set_temperature_offset(temperature_offset) != STATUS_OK) {
    Support::Logger::info("SCD30-ACTOR", "Could not set calibration offset");
    send_failure_notification();
  }

  Support::Logger::info("SCD30-ACTOR", "Calibrated. Offset: %d",
                        temperature_offset);

  calibrated_temp = true;
  current_offset_temp = temperature_offset;

  // We try try to minimize the writes to the sensor and store the required
  // information directly on the ESP32
  write_temperature_calibration_value(temperature_offset);
}

void SCD30Actor::co2_forced_calibration(uint16_t ppm) {
  Support::Logger::info("SCD30-ACTOR", "Start CO2 calibration");
  update_calibration_notification("calibration\nstart");

  int uptime = BoardFunctions::seconds_timestamp() - init_timestamp;

  if (uptime < 300) {
    Support::Logger::info("SCD30-ACTOR",
                          "Calibration: Sensor not warmed up yet (Uptime: %d)",
                          uptime);
    update_calibration_notification("calibration\nfailure:\nuptime");
    return;
  }

  if (scd30_probe() != STATUS_OK) {
    Support::Logger::error("SCD30-ACTOR", "Calibration: Sensor not found");
    update_calibration_notification("calibration\nfailure:\nprobe");
    return;
  }
  // return;

  if (scd30_set_forced_recalibration(ppm) != STATUS_OK) {
    Support::Logger::error("SCD30-ACTOR", "Calibration: FRC failed");
    update_calibration_notification("calibration\nfailure:\nfrc");
    return;
  }

  calibrated_co2 = true;

  time_t t = 0;
  time(&t);
  write_co2_calibration_timestamp(static_cast<uint32_t>(t));
  update_calibration_notification("calibrated");
}

std::pair<bool, uint16_t> SCD30Actor::read_temperature_calibrarion_value() {
  nvs_handle_t storage_handle;
  if (nvs_open("sensor_c", NVS_READWRITE, &storage_handle) != ESP_OK) {
    Support::Logger::error("SCD30-ACTOR", "Stored calibration read error");
    send_exit_message();
    nvs_close(storage_handle);
    return std::make_pair(false, 0);
  }
  uint16_t calibration_value = 0;
  auto err = nvs_get_u16(storage_handle, "scd30_t_o", &calibration_value);
  if (err == ESP_OK) {
    nvs_close(storage_handle);
    return std::make_pair(true, calibration_value);
  } else {
    if (err != ESP_ERR_NVS_NOT_FOUND) {
      Support::Logger::error("SCD30-ACTOR", "Stored calibration read error");
      send_exit_message();
    }
    nvs_close(storage_handle);
    return std::make_pair(false, 0);
  }
}

void SCD30Actor::write_temperature_calibration_value(
    uint16_t calibration_value) {
  nvs_handle_t storage_handle;
  if (nvs_open("sensor_c", NVS_READWRITE, &storage_handle) != ESP_OK) {
    Support::Logger::error("SCD30-ACTOR", "Calibration write error: Open");
  }
  if (nvs_set_u16(storage_handle, "scd30_t_o", calibration_value) != ESP_OK) {
    Support::Logger::error("SCD30-ACTOR", "Calibration write error: Write");
  }
  if (nvs_commit(storage_handle) != ESP_OK) {
    Support::Logger::error("SCD30-ACTOR", "Calibration write error: Commit");
  }
  nvs_close(storage_handle);
}

std::pair<bool, uint32_t> SCD30Actor::read_co2_configuration_timestamp() {
  nvs_handle_t storage_handle;
  if (nvs_open("sensor_c", NVS_READWRITE, &storage_handle) != ESP_OK) {
    Support::Logger::error("SCD30-ACTOR",
                           "Stored calibration timestamp read error");
    send_exit_message();
    nvs_close(storage_handle);
    return std::make_pair(false, 0);
  }
  uint32_t calibration_value = 0;
  auto err = nvs_get_u32(storage_handle, "scd30_c_t", &calibration_value);
  if (err == ESP_OK) {
    nvs_close(storage_handle);
    return std::make_pair(true, calibration_value);
  } else {
    if (err != ESP_ERR_NVS_NOT_FOUND) {
      Support::Logger::error("SCD30-ACTOR",
                             "Stored calibration timestamp read error");
      send_exit_message();
    }
    nvs_close(storage_handle);
    return std::make_pair(false, 0);
  }
}

void SCD30Actor::write_co2_calibration_timestamp(
    uint32_t calibration_timestamp) {
  nvs_handle_t storage_handle;
  if (nvs_open("sensor_c", NVS_READWRITE, &storage_handle) != ESP_OK) {
    Support::Logger::error("SCD30-ACTOR",
                           "Calibration timestamp write error: Open");
  }
  if (nvs_set_u32(storage_handle, "scd30_c_t", calibration_timestamp) !=
      ESP_OK) {
    Support::Logger::error("SCD30-ACTOR",
                           "Calibration timestamp write error: Write");
  }
  if (nvs_commit(storage_handle) != ESP_OK) {
    Support::Logger::error("SCD30-ACTOR",
                           "Calibration timestamp write error: Commit");
  }
  nvs_close(storage_handle);
}

void SCD30Actor::update_calibration_notification(
    std::string_view notification_text) {
  auto calibration_info = PubSub::Publication{};
  calibration_info.set_attr("type", "notification");
  calibration_info.set_attr("notification_text", notification_text);
  calibration_info.set_attr("notification_id", "calibrated");
  calibration_info.set_attr("notification_lifetime", 0);
  calibration_info.set_attr("node_id", node_id());

  publish(std::move(calibration_info));
}

}  // namespace uActor::ESP32::Sensors
