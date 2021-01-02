#ifndef UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_DATA_HPP_
#define UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_DATA_HPP_

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
    return std::move(old);
  }

  static void increase(const std::string& variable, float value) {
    std::shared_lock lck(mtx);
    instance.data[variable] += value;
  }

  static void decrease(const std::string& variable, float value) {
    std::shared_lock lck(mtx);
    instance.data[variable] -= value;
  }

  static void set(const std::string& variable, float value) {
    std::shared_lock lck(mtx);
    instance.data[variable] = value;
  }

  static TelemetryData& get_instance() { return instance; }

  std::map<std::string, float> data;

  static std::shared_mutex mtx;
  static TelemetryData instance;
};

}  // namespace uActor::Controllers

#endif  // UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_DATA_HPP_
