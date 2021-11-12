#pragma once

#include <libplatform/libplatform.h>
#include <v8.h>

#include <string>
#include <string_view>

#include "actor_runtime/code_identifier.hpp"
#include "actor_runtime/code_store.hpp"
#include "actor_runtime/managed_actor.hpp"
#include "actor_runtime/v8/actor_callbacks.hpp"
#include "actor_runtime/v8/publication_wrapper.hpp"
#include "pubsub/constraint.hpp"
#include "pubsub/filter.hpp"
#include "support/logger.hpp"

namespace uActor::ActorRuntime::V8Runtime {

// V8 binding derived from the process.cc example bundled with V8.
class ManagedV8Actor : public ActorRuntime::ManagedActor {
 public:
  template <typename PAllocator = allocator_type>
  ManagedV8Actor(ExecutorApi* api, uint32_t unique_id, std::string_view node_id,
                 std::string_view actor_type, std::string_view actor_version,
                 std::string_view instance_id, v8::Platform* platform,
                 PAllocator allocator = make_allocator<ManagedV8Actor>())
      : ManagedActor(api, unique_id, node_id, actor_type, actor_version,
                     instance_id, allocator),
        platform(platform) {}

  ~ManagedV8Actor();

  RuntimeReturnValue receive(PubSub::Publication&& m) override;

  std::string actor_runtime_type() override {
    return std::string("javascript");
  }

 protected:
  RuntimeReturnValue early_internal_initialize() override;

  RuntimeReturnValue late_internal_initialize(std::string&& code) override;

  // Not Implemented
  bool hibernate_internal() override { return false; };

  // Not Implemented
  RuntimeReturnValue wakeup_internal() override {
    return RuntimeReturnValue::OK;
  }

 private:
  v8::Platform* platform;
  v8::Isolate* isolate;
  v8::Global<v8::Context> actor_context;
  v8::Global<v8::Function> receive_function;
  v8::Global<v8::Object> actor_state;

  RuntimeReturnValue attempt_direct_init_or_fetch_code();

  RuntimeReturnValue createActorEnvironment(std::string_view code);

  v8::Local<v8::ObjectTemplate> actor_context_template();

  RuntimeReturnValue load_code(const v8::Local<v8::Context>& context,
                               std::string_view code);

  friend ActorClosures;
};

}  //  namespace uActor::ActorRuntime::V8Runtime
