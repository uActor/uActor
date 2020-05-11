#include "actor_runtime/managed_native_actor.hpp"

// TODO(raphaelhetzel) This is currently disabled as this requires a module system.
// #ifdef ESP_IDF
// #include "io/bmp180_actor.hpp"
// #endif

#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"

namespace uActor::ActorRuntime {
ManagedNativeActor::ManagedNativeActor(ExecutorApi* api, uint32_t unique_id,
                                       const char* node_id,
                                       const char* actor_type,
                                       const char* instance_id,
                                       const char* code)
    : ManagedActor(api, unique_id, node_id, actor_type, instance_id, code),
      actor(nullptr) {
  if (std::string_view("deployment_manager") == actor_type) {
    actor = std::make_unique<Controllers::DeploymentManager>(
        this, node_id, actor_type, instance_id);
  } else if (std::string_view("topology_manager") == actor_type) {
    actor = std::make_unique<Controllers::TopologyManager>(
        this, node_id, actor_type, instance_id);
// TODO(raphaelhetzel) This is currently disabled as this requires a module system.
// #ifdef ESP_IDF
//   } else if (std::string_view("bmp180_sensor") == actor_type) {
//     actor = std::make_unique<ESP32::IO::BMP180Actor>(this, node_id, actor_type,
//                                                      instance_id);
// #endif
  }
}

bool ManagedNativeActor::receive(const PubSub::Publication& p) {
  actor->receive(std::move(p));
  return true;
}
}  // namespace uActor::ActorRuntime
