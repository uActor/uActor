#pragma once

#include <set>
#include <string>
#include <utility>

#include "cluster_processor_utils.hpp"
#include "subscription_processor.hpp"

namespace uActor::Remote {

using KeyList = std::set<std::string>;

class ClusterBarrier : public SubscriptionProcessor {
 public:
  ClusterBarrier(std::string_view local_node_id, std::string_view peer_node_id,
                 ClusterLabels cluster_labels, KeyList drop_keys)
      : SubscriptionProcessor(local_node_id, peer_node_id),
        cluster_labels(std::move(cluster_labels)),
        drop_keys(std::move(drop_keys)) {
    for (const auto& [_key, value_pair] : this->cluster_labels) {
      if (value_pair.local_label != value_pair.remote_label) {
        same_cluster = false;
        break;
      }
    }
  }

  ~ClusterBarrier() = default;

  bool process_added(PubSub::Filter* filter,
                     const PubSub::SubscriptionArguments& arguments) override {
    if (!same_cluster) {
      for (const auto& key : drop_keys) {
        if (has_equality_constraint(filter, key)) {
          return true;
        }
      }
    }
    return false;
  }

  bool process_removed(
      PubSub::Filter* filter,
      const PubSub::SubscriptionArguments& arguments) override {
    if (!same_cluster) {
      for (const auto& key : drop_keys) {
        if (has_equality_constraint(filter, key)) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  bool has_equality_constraint(PubSub::Filter* filter, std::string_view key) {
    for (const auto& constraint : filter->required) {
      if (constraint.attribute() == key &&
          constraint.predicate() == PubSub::ConstraintPredicates::EQ) {
        return true;
      }
    }
    return false;
  }

  bool same_cluster = true;
  ClusterLabels cluster_labels;
  KeyList drop_keys;
};

}  // namespace uActor::Remote
