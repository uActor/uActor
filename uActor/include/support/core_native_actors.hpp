#pragma once

#include "actor_runtime/code_store_actor.hpp"
#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"

#if CONFIG_UACTOR_ENABLE_TELEMETRY
#include "controllers/telemetry_actor.hpp"
#endif

namespace uActor::Support {

struct CoreNativeActors {
  static void register_native_actors() {
#if CONFIG_UACTOR_ENABLE_TELEMETRY
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "telemetry_actor", ActorRuntime::NativeActor::create_instance<
                               Controllers::TelemetryActor>);
#endif
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "deployment_manager", ActorRuntime::NativeActor::create_instance<
                                  Controllers::DeploymentManager>);
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "topology_manager", ActorRuntime::NativeActor::create_instance<
                                Controllers::TopologyManager>);
    ActorRuntime::NativeActorRegistry::register_actor_type(
        "code_store", ActorRuntime::NativeActor::create_instance<
                          ActorRuntime::CodeStoreActor>);
  }
};

}  // namespace uActor::Support
