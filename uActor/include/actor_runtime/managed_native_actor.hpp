#ifndef MAIN_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_
#define MAIN_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_

#include <memory>
#include <utility>

#include "managed_actor.hpp"
#include "native_actor.hpp"

namespace uActor::ActorRuntime {

class ManagedNativeActor : public ManagedActor {
 public:
  ManagedNativeActor(ExecutorApi* api, uint32_t unique_id, const char* node_id,
                     const char* actor_type, const char* instance_id,
                     const char* code);

  bool receive(const PubSub::Publication& p);

  bool internal_initialize() { return true; }

 private:
  std::unique_ptr<NativeActor> actor;
  friend NativeActor;
};

}  //  namespace uActor::ActorRuntime

#endif  //  MAIN_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_
