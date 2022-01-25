#pragma once

#include <algorithm>
#include <set>
#include <utility>

#include "pubsub/constraint.hpp"
#include "subscription_processor.hpp"

namespace uActor::Remote {
class OptionalConstraintDrop : public SubscriptionProcessor {
 public:
  OptionalConstraintDrop(std::string_view local_node_id,
                         std::string_view peer_node_id,
                         std::set<PubSub::Constraint>&& keys_to_drop)
      : SubscriptionProcessor(local_node_id, peer_node_id),
        constraints_to_drop(std::move(keys_to_drop)) {}

  ~OptionalConstraintDrop() = default;

  bool process_added(PubSub::Filter* filter,
                     const PubSub::SubscriptionArguments& arguments) override {
    filter->optional.erase(
        std::remove_if(filter->optional.begin(), filter->optional.end(),
                       [this](const auto& item) {
                         return constraints_to_drop.find(item) !=
                                constraints_to_drop.end();
                       }),
        filter->optional.end());
    return false;
  }

  bool process_removed(
      PubSub::Filter* filter,
      const PubSub::SubscriptionArguments& arguments) override {
    filter->optional.erase(
        std::remove_if(filter->optional.begin(), filter->optional.end(),
                       [this](const auto& item) {
                         return constraints_to_drop.find(item) !=
                                constraints_to_drop.end();
                       }),
        filter->optional.end());
    return false;
  }

  std::set<PubSub::Constraint> constraints_to_drop;
};

}  // namespace uActor::Remote
