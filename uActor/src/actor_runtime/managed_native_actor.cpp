#include "actor_runtime/managed_native_actor.hpp"

#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"

namespace uActor::ActorRuntime {

bool ManagedNativeActor::receive(PubSub::Publication&& p) {
  actor->receive(std::move(p));
  return true;
}

std::unordered_map<std::string, ManagedNativeActor::ConstructionMethod>
    ManagedNativeActor::actor_constructors;
}  // namespace uActor::ActorRuntime
