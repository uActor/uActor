#pragma once

#include "subscription_processor.hpp"

namespace uActor::Remote {
class LeafNodeIdGateway : public SubscriptionProcessor {
 public:
  LeafNodeIdGateway(std::string_view local_node_id,
                    std::string_view peer_node_id)
      : SubscriptionProcessor(local_node_id, peer_node_id) {}

  ~LeafNodeIdGateway() = default;

  bool process_added(PubSub::Filter* filter) override {
    for (const auto& constraint : filter->required) {
      if (constraint.attribute() == "node_id" &&
          std::get<std::string_view>(constraint.operand()) != local_node_id &&
          constraint.predicate() == PubSub::ConstraintPredicates::EQ) {
        if (remote_node_id_count++ == 0) {
          *filter = PubSub::Filter{
              PubSub::Constraint{"node_id", local_node_id,
                                 PubSub::ConstraintPredicates::NE, false}};
          return false;
        } else {
          return true;
        }
      }
    }
    return false;
  }

  bool process_removed(PubSub::Filter* filter) override {
    for (const auto& constraint : filter->required) {
      if (constraint.attribute() == "node_id" &&
          std::get<std::string_view>(constraint.operand()) != local_node_id &&
          constraint.predicate() == PubSub::ConstraintPredicates::EQ) {
        if (--remote_node_id_count == 0) {
          *filter = PubSub::Filter{
              PubSub::Constraint{"node_id", local_node_id,
                                 PubSub::ConstraintPredicates::NE, false}};
          return false;
        } else {
          return true;
        }
      }
    }
    return false;
  }

 private:
  uint32_t remote_node_id_count = 0;
};

}  // namespace uActor::Remote
