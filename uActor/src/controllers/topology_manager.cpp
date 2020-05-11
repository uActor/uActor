#include "controllers/topology_manager.hpp"

#include <utility>

namespace uActor::Controllers {
TopologyManager::TopologyManager(
    ActorRuntime::ManagedNativeActor* actor_wrapper, std::string_view node_id,
    std::string_view actor_type, std::string_view instance_id)
    : ActorRuntime::NativeActor(actor_wrapper, node_id, actor_type,
                                instance_id) {
  subscribe(PubSub::Filter{PubSub::Constraint{"type", "peer_announcement"}});
  subscribe(PubSub::Filter{
      PubSub::Constraint{"type", "peer_update"},
      PubSub::Constraint{"publisher_node_id", std::string(node_id)}});
}

void TopologyManager::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "peer_announcement") {
    receive_peer_announcement(publication);
  } else if (publication.get_str_attr("type") == "peer_update") {
    receive_peer_update(publication);
  }
}

void TopologyManager::receive_peer_announcement(
    const PubSub::Publication& publication) {
  if (publication.get_str_attr("peer_type") == "tcp_server" &&
      publication.has_attr("peer_ip") && publication.has_attr("peer_port") &&
      publication.has_attr("peer_node_id")) {
    std::string peer_node_id =
        std::string(*publication.get_str_attr("peer_node_id"));
    if (should_connect(std::string_view(peer_node_id))) {
      PubSub::Publication connect{};
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

void TopologyManager::receive_peer_update(
    const PubSub::Publication& publication) {
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

bool TopologyManager::should_connect(std::string_view peer_id) {
  if (reachable_peers.find(std::string(peer_id)) == reachable_peers.end()) {
    
    // todo(raphaelhetzel) Store the server nodes directly in the topology manager (message-based interface)
    if (std::find(BoardFunctions::SERVER_NODES.begin(), BoardFunctions::SERVER_NODES.end(), peer_id) != BoardFunctions::SERVER_NODES.end()) {
      return true;
    }
  }
  return false;
}
}  // namespace uActor::Controllers
