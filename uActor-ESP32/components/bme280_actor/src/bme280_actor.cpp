#include "bme280_actor.hpp"

#include <set>
#include <string_view>
#include <utility>

namespace uActor::ESP32::Sensors {

BME280Actor::BME280Actor(ActorRuntime::ManagedNativeActor *actor_wrapper,
                         std::string_view node_id, std::string_view actor_type,
                         std::string_view instance_id)
    : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {
  dev.intf_ptr = &dev_addr;
  dev.intf = BME280_I2C_INTF;
  dev.read = &user_i2c_read;
  dev.write = &user_i2c_write;
  dev.delay_us = &user_delay_us;
}

BME280Actor::~BME280Actor() {
  for (const auto &actor_type : registered_actors) {
    deregister_managed_actor(actor_type);
  }
  registered_actors.clear();
}

void BME280Actor::receive(const PubSub::Publication &publication) {
  if (publication.get_str_attr("type") == "init" ||
      publication.get_str_attr("type") == "trigger_sensor_wait") {
    auto ret = bme280_init(&dev);
    if (ret != BME280_OK) {
      Support::Logger::warning("BME280", "INIT", "Sensor init error %d\n", ret);
      if (ready_retries < 5) {
        send_delayed_trigger(1000, "trigger_sensor_wait");
      } else {
        send_exit_message();
      }
    } else {
      uint8_t settings_sel;

      /* Recommended mode of operation: Indoor navigation */
      dev.settings.osr_h = BME280_OVERSAMPLING_1X;
      dev.settings.osr_p = BME280_OVERSAMPLING_16X;
      dev.settings.osr_t = BME280_OVERSAMPLING_2X;
      dev.settings.filter = BME280_FILTER_COEFF_16;
      dev.settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

      settings_sel = BME280_OSR_PRESS_SEL;
      settings_sel |= BME280_OSR_TEMP_SEL;
      settings_sel |= BME280_OSR_HUM_SEL;
      settings_sel |= BME280_STANDBY_SEL;
      settings_sel |= BME280_FILTER_SEL;

      if (bme280_set_sensor_settings(settings_sel, &dev)) {
        Support::Logger::warning("BME280", "INIT", "Sensor settings error\n");
        send_exit_message();
        return;
      }
      if (bme280_set_sensor_mode(BME280_NORMAL_MODE, &dev)) {
        Support::Logger::warning("BME280", "INIT", "Sensor mode error\n");
        send_exit_message();
        return;
      }

      register_unmanaged_actor("core.sensors.temperature");
      register_unmanaged_actor("core.sensors.relative_humidity");
      register_unmanaged_actor("core.sensors.pressure");
      send_delayed_trigger(100, "trigger_measurement");
    }
  } else if (publication.get_str_attr("type") == "exit") {
    for (auto actor_type : registered_actors) {
      deregister_managed_actor(actor_type);
    }
    registered_actors.clear();
  } else if (publication.get_str_attr("type") == "trigger_measurement") {
    fetch_send_updates();
  }
}

void BME280Actor::fetch_send_updates() {
  int8_t rslt;
  bme280_data comp_data;
  if (bme280_get_sensor_data(BME280_ALL, &comp_data, &dev)) {
    Support::Logger::warning("BME280", "FETCH", "Ready error\n");
    send_failure_notification();
  } else {
    send_update("temperature", "degree_celsius", comp_data.temperature);
    send_update("pressure", "pa", comp_data.pressure);
    send_update("relative_humidity", "percent", comp_data.humidity);
  }
  send_delayed_trigger(10000, "trigger_measurement");
}

void BME280Actor::send_update(std::string variable, std::string unit,
                              float value) {
  PubSub::Publication sensor_update;
  sensor_update.set_attr("type", std::string("sensor_update_" + variable));
  sensor_update.set_attr("value", value);
  sensor_update.set_attr("sensor", "bme280");
  sensor_update.set_attr("unit", unit);
  publish(std::move(sensor_update));
}

void BME280Actor::send_failure_notification() {
  PubSub::Publication sensor_failure;
  sensor_failure.set_attr("type", "sensor_failure");
  sensor_failure.set_attr("sensor", "bme280");
  publish(std::move(sensor_failure));
}

void BME280Actor::send_delayed_trigger(uint32_t delay, std::string type) {
  PubSub::Publication trigger;
  trigger.set_attr("type", type);
  trigger.set_attr("node_id", node_id().data());
  trigger.set_attr("actor_type", actor_type().data());
  trigger.set_attr("instance_id", instance_id().data());
  delayed_publish(std::move(trigger), delay);
}

void BME280Actor::send_exit_message() {
  PubSub::Publication exit_message;
  exit_message.set_attr("node_id", node_id());
  exit_message.set_attr("actor_type", actor_type());
  exit_message.set_attr("instance_id", instance_id());
  exit_message.set_attr("type", "exit");
  publish(std::move(exit_message));
}

void BME280Actor::register_unmanaged_actor(std::string type) {
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

void BME280Actor::deregister_managed_actor(std::string type) {
  PubSub::Publication deregister_message;
  deregister_message.set_attr("type", "unmanaged_actor_update");
  deregister_message.set_attr("command", "deregister");
  deregister_message.set_attr("update_actor_type", type);
  deregister_message.set_attr("node_id", node_id());
  publish(std::move(deregister_message));
}

void BME280Actor::user_delay_us(uint32_t period, void *intf_ptr) {
  vTaskDelay(period / 1000 / portTICK_PERIOD_MS);
}

int8_t BME280Actor::user_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                  uint32_t len, void *intf_ptr) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  uint8_t addr_b = BME280_I2C_ADDR_PRIM << 1;
  if (i2c_master_start(cmd) != ESP_OK) {
    return -1;
  }
  if (i2c_master_write_byte(cmd, addr_b | I2C_MASTER_WRITE, true) != ESP_OK)
    return -2;
  if (i2c_master_write_byte(cmd, reg_addr, true) != ESP_OK) {
    return -3;
  }
  if (i2c_master_start(cmd) != ESP_OK) {
    return -4;
  }
  if (i2c_master_write_byte(cmd, addr_b | I2C_MASTER_READ, true) != ESP_OK) {
    return -5;
  }
  if (i2c_master_read(cmd, reg_data, len, I2C_MASTER_LAST_NACK) != ESP_OK) {
    return -6;
  }
  if (i2c_master_stop(cmd) != ESP_OK) {
    return -7;
  }
  if (i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS) != ESP_OK) {
    return -8;
  }
  i2c_cmd_link_delete(cmd);
  return 0;
}

int8_t BME280Actor::user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
                                   uint32_t len, void *intf_ptr) {
  uint8_t addr_b = BME280_I2C_ADDR_PRIM << 1;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  if (i2c_master_start(cmd) != ESP_OK) {
    return -1;
  }
  if (i2c_master_write_byte(cmd, addr_b | I2C_MASTER_WRITE, true) != ESP_OK) {
    return -2;
  }
  if (i2c_master_write_byte(cmd, reg_addr, true) != ESP_OK) {
    return -3;
  }
  if (i2c_master_write(cmd, reg_data, len, true) != ESP_OK) {
    return -4;
  }
  if (i2c_master_stop(cmd) != ESP_OK) {
    return -5;
  }
  if (i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS)) {
    return -6;
  }
  i2c_cmd_link_delete(cmd);
  return 0;
}

}  // namespace uActor::ESP32::Sensors
