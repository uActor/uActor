#pragma once

#include <sdkconfig.h>

#if CONFIG_ENABLE_BMP180
#include "bmp180_actor.hpp"
#endif

#if CONFIG_ENABLE_BME280
#include "bme280_actor.hpp"
#endif

#if CONFIG_ENABLE_SCD30
#include "scd30_actor.hpp"
#endif

#if CONFIG_ENABLE_EPAPER_DISPLAY
#include "epaper_actor.hpp"
#endif

namespace uActor::Support {

struct ESP32NativeActors {
  static void register_native_actors() {
#if CONFIG_ENABLE_BMP180
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "bmp180_sensor",
        ActorRuntime::NativeActor::create_instance<ESP32::IO::BMP180Actor>);
#endif
#if CONFIG_ENABLE_BME280
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "bme280_sensor",
        ActorRuntime::NativeActor::create_instance<ESP32::Sensors::BME280Actor>);
#endif
#if CONFIG_ENABLE_SCD30
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "scd30_sensor",
        ActorRuntime::NativeActor::create_instance<ESP32::Sensors::SCD30Actor>);
#endif
#if CONFIG_ENABLE_EPAPER_DISPLAY
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "epaper_notification_actor",
        ActorRuntime::NativeActor::create_instance<
            ESP32::Notifications::EPaperNotificationActor>);
#endif
  }
};
}  // namespace uActor::Support
