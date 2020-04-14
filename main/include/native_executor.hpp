#ifndef MAIN_INCLUDE_NATIVE_EXECUTOR_HPP_
#define MAIN_INCLUDE_NATIVE_EXECUTOR_HPP_

#include <utility>

#include "executor.hpp"
#include "managed_actor.hpp"
#include "managed_native_actor.hpp"

class NativeExecutor : public Executor<ManagedNativeActor, NativeExecutor> {
 public:
  NativeExecutor(uActor::PubSub::Router* router, const char* node_id,
                const char* id)
      : Executor<ManagedNativeActor, NativeExecutor>(router, node_id,
                                                        "native_runtime", id) {}

  ~NativeExecutor() { actors.clear(); }

 private:
  void add_actor(uActor::PubSub::Publication&& publication) {
    add_actor_base(publication);
  }

  friend Executor;
};

#endif  // MAIN_INCLUDE_NATIVE_EXECUTOR_HPP_
