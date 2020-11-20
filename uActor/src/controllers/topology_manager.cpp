#include "controllers/topology_manager.hpp"

#include <utility>

#include "support/logger.hpp"

namespace uActor::Controllers {

TopologyManager::TopologyManager(
    ActorRuntime::ManagedNativeActor* actor_wrapper, std::string_view node_id,
    std::string_view actor_type, std::string_view instance_id)
    : ActorRuntime::NativeActor(actor_wrapper, node_id, actor_type,
                                instance_id) {
  subscribe(PubSub::Filter{
      PubSub::Constraint("type", "peer_announcement"),
      PubSub::Constraint("node_id", std::string(this->node_id()),
                         PubSub::ConstraintPredicates::EQ, true)});
  subscribe(PubSub::Filter{
      PubSub::Constraint{"type", "peer_update"},
      PubSub::Constraint{"publisher_node_id", std::string(node_id)}});
  subscribe(PubSub::Filter{
      PubSub::Constraint("type", "add_static_peer"),
      PubSub::Constraint("node_id", std::string(this->node_id()))});
}

void TopologyManager::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "init") {
    {
      auto p = PubSub::Publication();
      p.set_attr("node_id", node_id());
      p.set_attr("actor_type", actor_type());
      p.set_attr("instance_id", instance_id());
      p.set_attr("type", "trigger_static_peer");
      publish(std::move(p));
    }
  } else if (publication.get_str_attr("type") == "trigger_static_peer") {
    publish_persistent_peers();
  } else if (publication.get_str_attr("type") == "peer_announcement") {
    receive_peer_announcement(publication);
  } else if (publication.get_str_attr("type") == "peer_update") {
    receive_peer_update(publication);
  } else if (publication.get_str_attr("type") == "add_static_peer") {
    if (publication.has_attr("peer_node_id") &&
        publication.has_attr("peer_ip") && publication.has_attr("peer_port")) {
      persistent_peers.emplace_back(TCPPeer(
          std::string(*publication.get_str_attr("peer_node_id")),
          std::string(*publication.get_str_attr("peer_ip")),
          static_cast<uint32_t>(*publication.get_int_attr("peer_port"))));
      if (!persistent_announcement_active && reachable_peers.empty()) {
        persistent_announcement_active = true;
      }
    }
  }
}

void TopologyManager::receive_peer_announcement(
    const PubSub::Publication& publication) {
  if (publication.get_str_attr("peer_type") == "tcp_server" &&
      publication.has_attr("peer_ip") && publication.has_attr("peer_port") &&
      publication.has_attr("peer_node_id")) {
    std::string peer_node_id =
        std::string(*publication.get_str_attr("peer_node_id"));
    if (should_connect(peer_node_id)) {
      PubSub::Publication connect{};
      connect.set_attr("type", "tcp_client_connect");
      connect.set_attr("node_id", node_id());
      connect.set_attr("peer_ip", *publication.get_str_attr("peer_ip"));
      connect.set_attr("peer_port", *publication.get_int_attr("peer_port"));
      publish(std::move(connect));

      connection_in_progress.emplace(*publication.get_str_attr("peer_node_id"),
                                     BoardFunctions::timestamp() + 5000);

      // deffered_block_for(
      //     PubSub::Filter{PubSub::Constraint{"type", "peer_update"},
      //                    PubSub::Constraint{"update_type", "peer_added"},
      //                    PubSub::Constraint{"peer_node_id", peer_node_id}},
      //     2000);
    }
  }
}

void TopologyManager::receive_peer_update(
    const PubSub::Publication& publication) {
  if (publication.get_str_attr("publisher_node_id") == node_id() &&
      publication.has_attr("update_type") &&
      publication.has_attr("peer_node_id")) {
    if (publication.get_str_attr("update_type") == "peer_added") {
      Support::Logger::info("TOPOLOGY-MANAGER", "PEER-UPDATE", "Peer added %s",
                            publication.get_str_attr("peer_node_id")->data());
      reachable_peers.emplace(*publication.get_str_attr("peer_node_id"));
      connection_in_progress.erase(
          std::string(*publication.get_str_attr("peer_node_id")));
      {
        PubSub::Publication p{};
        p.set_attr("type", "data_point");
        p.set_attr("measurement", "system");
        p.set_attr("values", "connections");
        p.set_attr("connections", 1);
        p.set_attr("tags", "node");
        p.set_attr("node", node_id());
        publish(std::move(p));
      }
    } else if (publication.get_str_attr("update_type") == "peer_removed") {
      Support::Logger::info("TOPOLOGY-MANAGER", "PEER-UPDATE",
                            "Peer removed %s",
                            publication.get_str_attr("peer_node_id")->data());
      {
        PubSub::Publication p{};
        p.set_attr("type", "data_point");
        p.set_attr("measurement", "system");
        p.set_attr("values", "connections");
        p.set_attr("connections", -1);
        p.set_attr("tags", "node");
        p.set_attr("node", node_id());
        publish(std::move(p));
      }
      if (!persistent_announcement_active && reachable_peers.empty()) {
        persistent_announcement_active = true;
      }
      reachable_peers.erase(
          std::string(*publication.get_str_attr("peer_node_id")));
    }
  }
}

void TopologyManager::publish_persistent_peers() {
  if (persistent_announcement_active) {
    for (const auto& peer : persistent_peers) {
      auto p = PubSub::Publication();
      p.set_attr("type", "peer_announcement");
      p.set_attr("peer_type", "tcp_server");
      p.set_attr("peer_node_id", peer.node_id);
      p.set_attr("peer_ip", peer.ip);
      p.set_attr("peer_port", peer.port);
      p.set_attr("node_id", node_id());
      publish(std::move(p));
    }
  }

  auto p = PubSub::Publication();
  p.set_attr("node_id", node_id());
  p.set_attr("actor_type", actor_type());
  p.set_attr("instance_id", instance_id());
  p.set_attr("type", "trigger_static_peer");
  delayed_publish(std::move(p), 30000);
}

bool TopologyManager::should_connect(const std::string& peer_id) {
  auto connection_attempt = connection_in_progress.find(peer_id);
  if (connection_attempt != connection_in_progress.end()) {
    if (connection_attempt->second < BoardFunctions::timestamp()) {
      connection_in_progress.erase(connection_attempt);
    } else {
      return false;
    }
  }
  if (reachable_peers.find(peer_id) == reachable_peers.end()) {
    // todo(raphaelhetzel) Store the server nodes directly in the topology
    // manager (message-based interface)
    if (std::find(BoardFunctions::SERVER_NODES.begin(),
                  BoardFunctions::SERVER_NODES.end(),
                  peer_id) != BoardFunctions::SERVER_NODES.end()) {
      return true;
    }
  }
  return false;
}
}  // namespace uActor::Controllers
