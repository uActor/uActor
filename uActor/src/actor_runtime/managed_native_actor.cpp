#include "actor_runtime/managed_native_actor.hpp"

#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"

namespace uActor::ActorRuntime {
ManagedNativeActor::ManagedNativeActor(ExecutorApi* api, uint32_t unique_id,
                                       std::string_view node_id,
                                       std::string_view actor_type,
                                       std::string_view actor_version,
                                       std::string_view instance_id)
    : ManagedActor(api, unique_id, node_id, actor_type, actor_version,
                   instance_id),
      actor(nullptr) {
  auto actor_constructor_it = actor_constructors.find(std::string(actor_type));
  if (actor_constructor_it != actor_constructors.end()) {
    actor = std::move(
        actor_constructor_it->second(this, node_id, actor_type, instance_id));
  } else {
    printf("Tried to spawn a native actor with unknown type.\n");
  }
}

bool ManagedNativeActor::receive(PubSub::Publication&& p) {
  actor->receive(std::move(p));
  return true;
}

std::unordered_map<std::string, ManagedNativeActor::ConstructionMethod>
    ManagedNativeActor::actor_constructors;
}  // namespace uActor::ActorRuntime
