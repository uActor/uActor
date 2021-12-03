#include "actor_runtime/managed_wasm_actor.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string_view>
#include <variant>
#include <vector>

#include "actor_runtime/code_store.hpp"
#include "pubsub/publication.hpp"
#include "support/logger.hpp"


namespace uActor::ActorRuntime {
ManagedActor::RuntimeReturnValue ManagedWasmActor::receive(
    PubSub::Publication&& _pub) {
  PubSub::Publication pub = _pub;

  try {
    wasm3::function recieve_func = this->runtime.find_function("recieve");
    wasm3::pointer_t publication =
        store_publication(this->runtime.get_memory(), pub);

    std::cout << "Pub_addr" << publication << std::endl;

    int32_t func_return = recieve_func.call<int32_t, int32_t>(publication);

    std::cout << "Func_ret" << func_return << std::endl;

  } catch (const wasm3::error& e) {
    Support::Logger::error("WASM ACTOR", "Error executing receive function: %s",
                           e.what());
    return ManagedActor::RuntimeReturnValue::RUNTIME_ERROR;
  }

  return ManagedActor::RuntimeReturnValue::OK;
}

bool ManagedWasmActor::hibernate_internal() { return false; }

ManagedActor::RuntimeReturnValue ManagedWasmActor::fetch_code_and_init() {
  auto result = CodeStore::get_instance().retrieve(
      CodeIdentifier(actor_type(), actor_version(), std::string_view("wasm")));
  if (not result.has_value()) {
    trigger_code_fetch();
    return RuntimeReturnValue::NOT_READY;
  }
  try {
    wasm3::module mod =
        env.parse_module(reinterpret_cast<const uint8_t*>(result->code.data()),
                         result->code.length());
    runtime.load(mod, reinterpret_cast<void*>(this));
    mod.link("_uactor", "malloc", uactor_malloc);
    mod.link("_uactor", "free", uactor_free);
    mod.link("_uactor", "print", uactor_print);
    mod.link("_uactor", "publish", uactor_publish);

    wasm3::function recieve_func = this->runtime.find_function("recieve");

  } catch (const wasm3::error& e) {
    Support::Logger::error("WASM ACTOR", "Error opening wasm module: %s",
                           e.what());
    return ManagedActor::RuntimeReturnValue::RUNTIME_ERROR;
  }

  return ManagedWasmActor::RuntimeReturnValue::OK;
}

void ManagedWasmActor::ext_publish(PubSub::Publication&& m) const {
  publish(std::move(m));
}

bool ManagedWasmActor::createActorEnvironment(
    std::string_view /*recieve_function*/) {
  return true;
}

ManagedActor::RuntimeReturnValue ManagedWasmActor::late_internal_initialize(
    std::string&& code) {
  if (!createActorEnvironment(std::move(code))) {
    return RuntimeReturnValue::INITIALIZATION_ERROR;
  } else {
    return RuntimeReturnValue::OK;
  }
}
}  // namespace uActor::ActorRuntime
