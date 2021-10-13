#include "actor_runtime/managed_native_actor.hpp"

#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"

namespace uActor::ActorRuntime {

ManagedActor::RuntimeReturnValue ManagedNativeActor::receive(
    PubSub::Publication&& p) {
  if (actor == nullptr) {
    return RuntimeReturnValue::INITIALIZATION_ERROR;
  }
  actor->receive(std::move(p));
  return RuntimeReturnValue::OK;
}

}  // namespace uActor::ActorRuntime
