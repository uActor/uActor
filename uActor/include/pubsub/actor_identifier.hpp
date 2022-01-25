#pragma once

#include <memory>
#include <string>
#include <utility>

#include "publication.hpp"

namespace uActor::PubSub {

struct ActorIdentifier {
  ActorIdentifier(std::string node_id, std::string actor_type,
                  std::string instance_id)
      : node_id(std::move(node_id)),
        actor_type(std::move(actor_type)),
        instance_id(std::move(instance_id)) {}

  std::string node_id;
  std::string actor_type;
  std::string instance_id;

  [[nodiscard]] std::shared_ptr<Publication::Map> to_publication_map() const;
  [[nodiscard]] static std::optional<ActorIdentifier> from_publication_map(
      const Publication::Map& map);
};

}  // namespace uActor::PubSub
