#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace uActor::ActorRuntime {

class NativeActor;
class ManagedNativeActor;

struct NativeActorRegistry {
  using ConstructionMethod = std::function<std::unique_ptr<NativeActor>(
      ManagedNativeActor* actor_wrapper, std::string_view node_id,
      std::string_view actor_type, std::string_view instance_id)>;

  static std::unordered_map<std::string, ConstructionMethod>&
  actor_constructors();

  // Basic type-registration system based on
  // https://www.bfilipek.com/2018/02/factory-selfregister.html
  // Self registering was not added as it did not work well with libraries
  static bool register_actor_type(std::string actor_type_name,
                                  ConstructionMethod&& construction_method);
};

}  // namespace uActor::ActorRuntime
