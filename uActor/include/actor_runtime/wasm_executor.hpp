#pragma once

#include "executor.hpp"
#include "managed_wasm_actor.hpp"

namespace uActor::ActorRuntime {
class WasmExecutor : public Executor<ManagedWasmActor, WasmExecutor> {
 public:
  WasmExecutor(uActor::PubSub::Router* router, const char* node_id,
               const char* id);

  ~WasmExecutor() override;
};
}  // namespace uActor::ActorRuntime
