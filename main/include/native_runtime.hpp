#ifndef MAIN_INCLUDE_NATIVE_RUNTIME_HPP_
#define MAIN_INCLUDE_NATIVE_RUNTIME_HPP_

#include <utility>

#include "actor_runtime.hpp"
#include "managed_actor.hpp"
#include "managed_native_actor.hpp"

class NativeRuntime : public ActorRuntime<ManagedNativeActor, NativeRuntime> {
 public:
  NativeRuntime(PubSub::Router* router, const char* node_id, const char* id)
      : ActorRuntime<ManagedNativeActor, NativeRuntime>(router, node_id,
                                                        "native_runtime", id) {}

  ~NativeRuntime() { actors.clear(); }

 private:
  void add_actor(Publication&& publication) { add_actor_base(publication); }

  friend ActorRuntime;
};

#endif  // MAIN_INCLUDE_NATIVE_RUNTIME_HPP_
