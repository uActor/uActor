#pragma once

#include <string>
#include <utility>

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
};

}  // namespace uActor::PubSub
