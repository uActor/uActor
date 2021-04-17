#pragma once

#include <memory>
#include <string>
#include <string_view>
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

  template <typename PAllocator = allocator_type>
  ManagedNativeActor(
      ExecutorApi* api, uint32_t unique_id, std::string_view node_id,
      std::string_view actor_type, std::string_view actor_version,
      std::string_view instance_id,
      PAllocator allocator = make_allocator<ManagedNativeActor>())
      : ManagedActor(api, unique_id, node_id, actor_type, actor_version,
                     instance_id, allocator),
        actor(nullptr) {
    auto actor_constructor_it =
        actor_constructors.find(std::string(actor_type));
    if (actor_constructor_it != actor_constructors.end()) {
      actor = std::move(
          actor_constructor_it->second(this, node_id, actor_type, instance_id));
    } else {
      printf("Tried to spawn a native actor with unknown type.\n");
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
