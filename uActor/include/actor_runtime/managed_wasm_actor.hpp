#pragma once

#include <string>

#include "managed_actor.hpp"
#include "wasm3.hpp"
#include "wasm_functions.hpp"

namespace uActor::ActorRuntime {

class ManagedWasmActor : public ManagedActor {
 private:
  wasm3::environment env;
  wasm3::runtime runtime = env.new_runtime(4096);

 public:
  template <typename PAllocator = allocator_type>
  ManagedWasmActor(ExecutorApi* api, uint32_t unique_id,
                   std::string_view node_id, std::string_view actor_type,
                   std::string_view actor_version, std::string_view instance_id,
                   PAllocator allocator = make_allocator<ManagedWasmActor>())
      : ManagedActor(api, unique_id, node_id, actor_type, actor_version,
                     instance_id, allocator) {}

  ~ManagedWasmActor() = default;

  RuntimeReturnValue receive(PubSub::Publication&& m) override;

  std::string actor_runtime_type() override { return {"wasm"}; }

  RuntimeReturnValue early_internal_initialize() override {
    return fetch_code_and_init();
  }

  RuntimeReturnValue late_internal_initialize(std::string&& code) override;

  bool hibernate_internal() override;

  RuntimeReturnValue wakeup_internal() override {
    return fetch_code_and_init();
  }

  // We must be able to call publish and subscribe from a static context,
  // therefore we add this static proxy methods
  void ext_publish(PubSub::Publication&& m);
  void ext_subscribe(PubSub::Filter&& f);

 private:
  RuntimeReturnValue fetch_code_and_init();
  bool createActorEnvironment(std::string_view recieve_function);
};
}  // namespace uActor::ActorRuntime
