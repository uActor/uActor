#pragma once

#include <string>
#include <utility>

#include "pubsub/publication.hpp"

namespace uActor::Remote {

class ForwardingStrategy {
 public:
  // Incomming
  virtual bool should_accept(const PubSub::Publication& p) = 0;
  virtual void add_incomming_routing_fields(PubSub::Publication* p) = 0;

  // Outgoing
  virtual bool should_forward(const PubSub::Publication& p) = 0;
  virtual void add_outgoing_routing_fields(PubSub::Publication* p) = 0;

  virtual ~ForwardingStrategy() {}

  void partner_node_id(std::string node_id) {
    _partner_node_id = std::move(node_id);
  }

  const std::string& partner_node_id() const { return _partner_node_id; }

 private:
  std::string _partner_node_id;
};

}  // namespace uActor::Remote

