#include "pubsub/actor_identifier.hpp"

namespace uActor::PubSub {
[[nodiscard]] std::shared_ptr<Publication::Map>
ActorIdentifier::to_publication_map() const {
  auto map = std::make_shared<Publication::Map>();
  map->set_attr("node_id", node_id);
  map->set_attr("actor_type", actor_type);
  map->set_attr("instance_id", instance_id);
  return map;
}

std::optional<ActorIdentifier> ActorIdentifier::from_publication_map(
    const Publication::Map& map) {
  auto raw_node_id = map.get_str_attr("node_id");
  auto raw_actor_type = map.get_str_attr("actor_type");
  auto raw_instance_id = map.get_str_attr("instance_id");
  if (!(raw_node_id && raw_actor_type && raw_instance_id)) {
    return std::nullopt;
  }

  return PubSub::ActorIdentifier(std::string(*raw_node_id),
                                 std::string(*raw_actor_type),
                                 std::string(*raw_instance_id));
}

}  // namespace uActor::PubSub
