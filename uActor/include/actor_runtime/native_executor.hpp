#pragma once

#include <utility>

#include "executor.hpp"
#include "managed_actor.hpp"
#include "managed_native_actor.hpp"

namespace uActor::ActorRuntime {

class NativeExecutor : public Executor<ManagedNativeActor, NativeExecutor> {
 public:
  NativeExecutor(uActor::PubSub::Router* router, const char* node_id,
                 const char* id)
      : Executor<ManagedNativeActor, NativeExecutor>(router, node_id,
                                                     "native_executor", id) {}

  ~NativeExecutor() { actors.clear(); }

 private:
  void add_actor(PubSub::Publication&& publication) {
    add_actor_base(publication);
  }

  friend Executor;
};

}  //  namespace uActor::ActorRuntime
