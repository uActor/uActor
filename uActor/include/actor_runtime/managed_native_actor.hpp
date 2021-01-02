#pragma once

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

  ManagedNativeActor(ExecutorApi* api, uint32_t unique_id,
                     std::string_view node_id, std::string_view actor_type,
                     std::string_view actor_version,
                     std::string_view instance_id);

  bool receive(PubSub::Publication&& p) override;

  std::string actor_runtime_type() override { return std::string("native"); }

  bool early_internal_initialize() override { return true; }
  bool late_internal_initialize(std::string&& /*_code*/) override {
    return true;
  }
  bool hibernate_internal() override { return false; };
  bool wakeup_internal() override { return true; };

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
