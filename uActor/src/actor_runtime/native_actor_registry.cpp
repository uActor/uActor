#include "actor_runtime/native_actor_registry.hpp"

#include <utility>

namespace uActor::ActorRuntime {

std::unordered_map<std::string, NativeActorRegistry::ConstructionMethod>&
NativeActorRegistry::actor_constructors() {
  static std::unordered_map<std::string,
                            NativeActorRegistry::ConstructionMethod>
      static_storage;
  return static_storage;
}

bool NativeActorRegistry::register_actor_type(
    std::string actor_type_name, ConstructionMethod&& construction_method) {
  actor_constructors().try_emplace(actor_type_name,
                                   std::move(construction_method));
  return true;
}
}  // namespace uActor::ActorRuntime
