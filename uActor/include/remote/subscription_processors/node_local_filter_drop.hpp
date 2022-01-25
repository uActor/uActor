#pragma once

#include "pubsub/constraint.hpp"
#include "subscription_processor.hpp"

namespace uActor::Remote {
class NodeLocalFilterDrop : public SubscriptionProcessor {
 public:
  NodeLocalFilterDrop(std::string_view local_node_id,
                      std::string_view peer_node_id)
      : SubscriptionProcessor(local_node_id, peer_node_id) {}

  ~NodeLocalFilterDrop() = default;

  bool process_added(PubSub::Filter* filter,
                     const PubSub::SubscriptionArguments& arguments) override {
    for (const auto& constraint : filter->required) {
      if (constraint.attribute() == "publisher_node_id") {
        if (std::holds_alternative<std::string_view>(constraint.operand()) &&
            std::get<std::string_view>(constraint.operand()) == local_node_id &&
            constraint.predicate() == PubSub::ConstraintPredicates::EQ) {
          return true;
        }
      }
    }
    return false;
  }

  bool process_removed(
      PubSub::Filter* filter,
      const PubSub::SubscriptionArguments& arguments) override {
    for (const auto& constraint : filter->required) {
      if (constraint.attribute() == "publisher_node_id") {
        if (std::holds_alternative<std::string_view>(constraint.operand()) &&
            std::get<std::string_view>(constraint.operand()) == local_node_id &&
            constraint.predicate() == PubSub::ConstraintPredicates::EQ) {
          return true;
        }
      }
    }
    return false;
  }
};

}  // namespace uActor::Remote
