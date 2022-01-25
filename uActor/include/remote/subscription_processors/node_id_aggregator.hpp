#pragma once

#include "subscription_processor.hpp"

namespace uActor::Remote {
class NodeIdAggregator : public SubscriptionProcessor {
 public:
  NodeIdAggregator(std::string_view local_node_id,
                   std::string_view peer_node_id)
      : SubscriptionProcessor(local_node_id, peer_node_id) {}

  ~NodeIdAggregator() = default;

  bool process_added(PubSub::Filter* filter,
                     const PubSub::SubscriptionArguments& arguments) override {
    for (const auto& constraint : filter->required) {
      if (constraint.attribute() == "node_id" &&
          std::get<std::string_view>(constraint.operand()) == local_node_id &&
          constraint.predicate() == PubSub::ConstraintPredicates::EQ) {
        if (node_id_count++ == 0) {
          *filter =
              PubSub::Filter{PubSub::Constraint{"node_id", local_node_id}};
          return false;
        } else {
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
      if (constraint.attribute() == "node_id" &&
          std::get<std::string_view>(constraint.operand()) == local_node_id &&
          constraint.predicate() == PubSub::ConstraintPredicates::EQ) {
        if (--node_id_count == 0) {
          *filter =
              PubSub::Filter{PubSub::Constraint{"node_id", local_node_id}};
          return false;
        } else {
          return true;
        }
      }
    }
    return false;
  }

 private:
  uint32_t node_id_count = 0;
};

}  // namespace uActor::Remote
