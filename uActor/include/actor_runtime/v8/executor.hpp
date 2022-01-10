#pragma once

#include <libplatform/libplatform.h>
#include <v8.h>

#include <cassert>
#include <memory>

#include "actor_runtime/executor.hpp"
#include "actor_runtime/v8/managed_actor.hpp"

namespace uActor::ActorRuntime::V8Runtime {

class V8Executor : public Executor<ManagedV8Actor, V8Executor> {
 public:
  V8Executor(uActor::PubSub::Router* router, const char* node_id,
             const char* id)
      : uActor::ActorRuntime::Executor<ManagedV8Actor, V8Executor>(
            router, node_id, "v8_executor", id) {
    start_v8();
    register_actor_type();
  }

  ~V8Executor() {
    // TODO(raphaelhetzel) V8 is currently not stopped
    deregister_actor_type();
    actors.clear();
  }

 private:
  static std::unique_ptr<v8::Platform> platform;
  static bool instance_running;

  void add_actor(uActor::PubSub::Publication&& publication);

  void register_actor_type();
  void deregister_actor_type();

  void start_v8();

  friend Executor;
};

}  // namespace uActor::ActorRuntime::V8Runtime
