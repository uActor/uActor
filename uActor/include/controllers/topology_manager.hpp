#pragma once

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif

#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

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
  struct TCPPeer {
    TCPPeer(std::string node_id, std::string ip, uint16_t port)
        : node_id(node_id), ip(ip), port(port) {}
    std::string node_id;
    std::string ip;
    uint16_t port;
  };

  std::unordered_set<std::string> reachable_peers;
  std::map<std::string, uint32_t> connection_in_progress;
  std::vector<TCPPeer> persistent_peers;
  bool persistent_announcement_active = false;

  void receive_peer_announcement(const PubSub::Publication& publication);

  void receive_peer_update(const PubSub::Publication& publication);

  bool should_connect(const std::string& peer_id, bool forced_mode);

  void publish_persistent_peers();
};

}  // namespace uActor::Controllers
