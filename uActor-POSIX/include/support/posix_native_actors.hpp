#pragma once

#if CONFIG_UACTOR_ENABLE_HTTP_INGRESS
#include "actors/http_ingress.hpp"
#endif

#if CONFIG_UACTOR_ENABLE_INFLUXDB_ACTOR
#include "actors/influxdb_actor.hpp"
#endif

namespace uActor::Support {

struct PosixNativeActors {
  static void register_native_actors() {
#if CONFIG_UACTOR_ENABLE_HTTP_INGRESS
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "http_ingress",
        ActorRuntime::NativeActor::create_instance<Ingress::HTTPIngress>);
#endif
#if CONFIG_UACTOR_ENABLE_INFLUXDB_ACTOR
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "influxdb_connector",
        ActorRuntime::NativeActor::create_instance<Database::InfluxDBActor>);
#endif
  }
};
}  // namespace uActor::Support
