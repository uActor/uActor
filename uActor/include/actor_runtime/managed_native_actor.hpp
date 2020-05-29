#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_

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

  bool receive(PubSub::Publication&& p) override;

  bool internal_initialize() { return true; }

 private:
  std::unique_ptr<NativeActor> actor;
  friend NativeActor;
};

}  //  namespace uActor::ActorRuntime

#endif  //  UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_NATIVE_ACTOR_HPP_
