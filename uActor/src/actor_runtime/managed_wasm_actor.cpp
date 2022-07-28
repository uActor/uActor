#include "actor_runtime/managed_wasm_actor.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string_view>
#include <variant>
#include <vector>

#include "actor_runtime/code_store.hpp"
#include "actor_runtime/wasm_functions.hpp"
#include "pubsub/publication.hpp"
#include "pubsub/router.hpp"
#include "support/logger.hpp"
#include "wasm3.hpp"

namespace uActor::ActorRuntime {
ManagedActor::RuntimeReturnValue ManagedWasmActor::receive(
    PubSub::Publication&& m) {
  try {
    wasm3::function recieve_func = this->runtime.find_function("receive");
    wasm3::pointer_t publication =
        get_publication_storage().insert(std::move(m));
    int32_t actor_ret = recieve_func.call<int32_t, wasm3::size_t>(publication);
    Support::Logger::info("WASM ACTOR", "Actor returned %i", actor_ret);
    get_publication_storage().erase(publication);
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
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        env.parse_module(reinterpret_cast<const uint8_t*>(result->code.data()),
                         result->code.length());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    runtime.load(mod, static_cast<void*>(this));
    mod.link("_uactor", "malloc", uactor_malloc);
    mod.link("_uactor", "memcpy", uactor_memcpy);
    mod.link("_uactor", "free", uactor_free);
    mod.link("_uactor", "print", uactor_print);
    mod.link("_uactor", "publish", uactor_publish);
    mod.link("_uactor", "get_val", uactor_get_val);
    mod.link("_uactor", "subscribe", uactor_subscribe);

    wasm3::function recieve_func = this->runtime.find_function("receive");
  } catch (const wasm3::error& e) {
    Support::Logger::error("WASM ACTOR", "Error opening wasm module: %s",
                           e.what());
    return ManagedActor::RuntimeReturnValue::RUNTIME_ERROR;
  }

  return ManagedWasmActor::RuntimeReturnValue::OK;
}

void ManagedWasmActor::ext_publish(PubSub::Publication&& m) {
  publish(std::move(m));
}

void ManagedWasmActor::ext_subscribe(PubSub::Filter&& f) {
  this->subscribe(std::move(f), {});
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
