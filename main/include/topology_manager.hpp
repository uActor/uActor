#ifndef MAIN_INCLUDE_TOPOLOGY_MANAGER_HPP_
#define MAIN_INCLUDE_TOPOLOGY_MANAGER_HPP_

#include <map>
#include <string>
#include <unordered_set>
#include <utility>

#include "native_actor.hpp"

class TopologyManager : public NativeActor {
 public:
  TopologyManager(ManagedNativeActor* actor_wrapper, std::string_view node_id,
                  std::string_view actor_type, std::string_view instance_id)
      : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {
    subscribe(PubSub::Filter{PubSub::Constraint{"type", "peer_announcement"}});
    subscribe(PubSub::Filter{
        PubSub::Constraint{"type", "peer_update"},
        PubSub::Constraint{"publisher_node_id", std::string(node_id)}});
  }

  void receive(const Publication& publication) {
    if (publication.get_str_attr("type") == "peer_announcement") {
      receive_peer_announcement(publication);
    } else if (publication.get_str_attr("type") == "peer_update") {
      receive_peer_update(publication);
    }
  }

 private:
  std::unordered_set<std::string> reachable_peers;

  void receive_peer_announcement(const Publication& publication) {
    if (publication.get_str_attr("peer_type") == "tcp_server" &&
        publication.has_attr("peer_ip") && publication.has_attr("peer_port") &&
        publication.has_attr("peer_node_id")) {
      std::string peer_node_id =
          std::string(*publication.get_str_attr("peer_node_id"));
      if (should_connect(std::string_view(peer_node_id))) {
        Publication connect{};
        connect.set_attr("type", "tcp_client_connect");
        connect.set_attr("node_id", node_id());
        connect.set_attr("peer_ip", *publication.get_str_attr("peer_ip"));
        connect.set_attr("peer_port", *publication.get_int_attr("peer_port"));
        publish(std::move(connect));
        deffered_block_for(
            PubSub::Filter{PubSub::Constraint{"type", "peer_update"},
                           PubSub::Constraint{"update_type", "peer_added"},
                           PubSub::Constraint{"peer_node_id", peer_node_id}},
            2000);
      }
    }
  }

  void receive_peer_update(const Publication& publication) {
    if (publication.get_str_attr("publisher_node_id") == node_id() &&
        publication.has_attr("update_type") &&
        publication.has_attr("peer_node_id")) {
      if (publication.get_str_attr("update_type") == "peer_added") {
        printf("peer added\n");
        reachable_peers.emplace(*publication.get_str_attr("peer_node_id"));
      } else if (publication.get_str_attr("update_type") == "peer_removed") {
        printf("peer removed\n");
        reachable_peers.erase(
            std::string(*publication.get_str_attr("peer_node_id")));
      }
    }
  }

  bool should_connect(std::string_view peer_id) {
    auto it = reachable_peers.find(std::string(peer_id));

    // star topology
    return it == reachable_peers.end() && peer_id != node_id() &&
           peer_id == "node_3";
  }
};

#endif  // MAIN_INCLUDE_TOPOLOGY_MANAGER_HPP_
