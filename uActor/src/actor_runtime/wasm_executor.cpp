#include "actor_runtime/wasm_executor.hpp"

namespace uActor::ActorRuntime {
WasmExecutor::WasmExecutor(PubSub::Router* router, const char* node_id,
                           const char* id)
    : Executor<ManagedWasmActor, WasmExecutor>(router, node_id, "wasm_executor",
                                               id) {
  PubSub::Publication p{BoardFunctions::NODE_ID, "wasm_executor", "1"};
  p.set_attr("type", "executor_update");
  p.set_attr("command", "register");
  p.set_attr("actor_runtime_type", "wasm");
  p.set_attr("update_node_id", BoardFunctions::NODE_ID);
  p.set_attr("update_actor_type", "wasm_executor");
  p.set_attr("update_instance_id", "1");
  p.set_attr("node_id", BoardFunctions::NODE_ID);
  PubSub::Router::get_instance().publish(std::move(p));
}

WasmExecutor::~WasmExecutor() {
  PubSub::Publication p{BoardFunctions::NODE_ID, "wasm_executor", "1"};
  p.set_attr("type", "executor_update");
  p.set_attr("command", "deregister");
  p.set_attr("actor_runtime_type", "wasm");
  p.set_attr("update_node_id", BoardFunctions::NODE_ID);
  p.set_attr("update_actor_type", "wasm_executor");
  p.set_attr("update_instance_id", "1");
  p.set_attr("node_id", BoardFunctions::NODE_ID);
  uActor::PubSub::Router::get_instance().publish(std::move(p));

  actors.clear();
}
}  // namespace uActor::ActorRuntime
