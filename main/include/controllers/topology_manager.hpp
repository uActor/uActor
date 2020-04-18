#ifndef MAIN_INCLUDE_CONTROLLERS_TOPOLOGY_MANAGER_HPP_
#define MAIN_INCLUDE_CONTROLLERS_TOPOLOGY_MANAGER_HPP_

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif

#include <string>
#include <string_view>
#include <unordered_set>

#include "actor_runtime/native_actor.hpp"
#include "pubsub/publication.hpp"

namespace uActor::Controllers {

class TopologyManager : public ActorRuntime::NativeActor {
 public:
  TopologyManager(ActorRuntime::ManagedNativeActor* actor_wrapper,
                  std::string_view node_id, std::string_view actor_type,
                  std::string_view instance_id);

  void receive(const PubSub::Publication& publication);

 private:
  std::unordered_set<std::string> reachable_peers;

  void receive_peer_announcement(const PubSub::Publication& publication);

  void receive_peer_update(const PubSub::Publication& publication);

  bool should_connect(std::string_view peer_id);
};

}  // namespace uActor::Controllers

#endif  // MAIN_INCLUDE_CONTROLLERS_TOPOLOGY_MANAGER_HPP_
