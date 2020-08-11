#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "managed_actor.hpp"
#include "native_actor.hpp"

namespace uActor::ActorRuntime {

class ManagedNativeActor : public ManagedActor {
 public:
  using ConstructionMethod = std::function<std::unique_ptr<NativeActor>(
      ManagedNativeActor* actor_wrapper, std::string_view node_id,
      std::string_view actor_type, std::string_view instance_id)>;

  ManagedNativeActor(ExecutorApi* api, uint32_t unique_id, const char* node_id,
                     const char* actor_type, const char* instance_id,
                     const char* code);

  bool receive(PubSub::Publication&& p) override;

  bool internal_initialize() { return true; }

  // Basic type-registration system based on
  // https://www.bfilipek.com/2018/02/factory-selfregister.html
  // TODO(raphaelhetzel) Further reduce the API complexity
  template <typename ActorType>
  static void register_actor_type(std::string actor_type_name) {
    actor_constructors.try_emplace(
        actor_type_name,
        uActor::ActorRuntime::NativeActor::create_instance<ActorType>);
  }

 private:
  std::unique_ptr<NativeActor> actor;
  static std::unordered_map<std::string, ConstructionMethod> actor_constructors;
  friend NativeActor;
};

}  //  namespace uActor::ActorRuntime

#endif  //  UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_
