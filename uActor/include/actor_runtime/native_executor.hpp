#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_NATIVE_EXECUTOR_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_NATIVE_EXECUTOR_HPP_

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

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_NATIVE_EXECUTOR_HPP_
