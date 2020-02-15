#ifndef MAIN_INCLUDE_MANAGED_NATIVE_ACTOR_HPP_
#define MAIN_INCLUDE_MANAGED_NATIVE_ACTOR_HPP_

#include <memory>
#include <utility>

#include "deployment_manager.hpp"
#include "managed_actor.hpp"
#include "native_actor.hpp"
#include "topology_manager.hpp"

class ManagedNativeActor : public ManagedActor {
 public:
  ManagedNativeActor(RuntimeApi* api, uint32_t unique_id, const char* node_id,
                     const char* actor_type, const char* instance_id,
                     const char* code)
      : ManagedActor(api, unique_id, node_id, actor_type, instance_id, code),
        actor(nullptr) {
    if (std::string_view("deployment_manager") == actor_type) {
      actor = std::make_unique<DeploymentManager>(this, node_id, actor_type,
                                                  instance_id);
    } else if (std::string_view("topology_manager") == actor_type) {
      actor = std::make_unique<TopologyManager>(this, node_id, actor_type,
                                                instance_id);
    }
  }

  bool receive(const uActor::PubSub::Publication& p) {
    actor->receive(std::move(p));
    return true;
  }

  bool internal_initialize() { return true; }

 private:
  std::unique_ptr<NativeActor> actor;
  friend NativeActor;
};

#endif  //  MAIN_INCLUDE_MANAGED_NATIVE_ACTOR_HPP_
