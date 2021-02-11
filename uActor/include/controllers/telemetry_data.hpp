#ifndef UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_DATA_HPP_
#define UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_DATA_HPP_

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif

#include <map>
#include <shared_mutex>
#include <string>
#include <utility>

#include "actor_runtime/native_actor.hpp"

namespace uActor::Controllers {

struct TelemetryData {
  static TelemetryData replace_instance() {
    std::unique_lock lck(mtx);
    TelemetryData old = instance;
    instance.data.clear();
    return old;
  }

#if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
  static TelemetryData replace_seconds_instance() {
    std::unique_lock lck(mtx);
    TelemetryData old = seconds_instance;
    seconds_instance.data.clear();
    return old;
  }
#endif

  static void increase(const std::string& variable, float value) {
    std::shared_lock lck(mtx);
    instance.data[variable] += value;
#if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
    seconds_instance.data[variable] += value;
#endif
  }

  static void decrease(const std::string& variable, float value) {
    std::shared_lock lck(mtx);
    instance.data[variable] -= value;
#if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
    seconds_instance.data[variable] -= value;
#endif
  }

  static void set(const std::string& variable, float value) {
    std::shared_lock lck(mtx);
    instance.data[variable] = value;
#if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
    seconds_instance.data[variable] = value;
#endif
  }

  static TelemetryData& get_instance() { return instance; }
#if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
  static TelemetryData& get_seconds_instance() { return seconds_instance; }
#endif

  std::map<std::string, float> data;

  static std::shared_mutex mtx;

  static TelemetryData instance;
#if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
  static TelemetryData seconds_instance;
#endif
};

}  // namespace uActor::Controllers

#endif  // UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_DATA_HPP_
