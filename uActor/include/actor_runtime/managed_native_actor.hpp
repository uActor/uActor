#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "managed_actor.hpp"
#include "native_actor.hpp"
#include "native_actor_registry.hpp"
#include "support/logger.hpp"

namespace uActor::ActorRuntime {

class ManagedNativeActor : public ManagedActor {
 public:
  template <typename PAllocator = allocator_type>
  ManagedNativeActor(
      ExecutorApi* api, uint32_t unique_id, std::string_view node_id,
      std::string_view actor_type, std::string_view actor_version,
      std::string_view instance_id,
      PAllocator allocator = make_allocator<ManagedNativeActor>())
      : ManagedActor(api, unique_id, node_id, actor_type, actor_version,
                     instance_id, allocator),
        actor(nullptr) {
    const auto& constructors = NativeActorRegistry::actor_constructors();
    auto actor_constructor_it = constructors.find(std::string(actor_type));
    if (actor_constructor_it != constructors.end()) {
      actor = std::move(
          actor_constructor_it->second(this, node_id, actor_type, instance_id));
    } else {
      Support::Logger::warning(
          "NATIVE-ACTOR", "Tried to spawn a native actor with unknown type.");
    }
  }

  RuntimeReturnValue receive(PubSub::Publication&& p) override;

  std::string actor_runtime_type() override { return std::string("native"); }

  RuntimeReturnValue early_internal_initialize() override {
    return RuntimeReturnValue::OK;
  }
  RuntimeReturnValue late_internal_initialize(
      std::string&& /*_code*/) override {
    return RuntimeReturnValue::OK;
  }
  bool hibernate_internal() override { return false; };
  RuntimeReturnValue wakeup_internal() override {
    return RuntimeReturnValue::OK;
  };

 private:
  std::unique_ptr<NativeActor> actor;
  friend NativeActor;
};

}  //  namespace uActor::ActorRuntime
