#pragma once

#include <iostream>
#include <utility>
#include <vector>

#include "cluster_processor_utils.hpp"
#include "subscription_processor.hpp"

namespace uActor::Remote {

class ClusterAggregator : public SubscriptionProcessor {
 public:
  ClusterAggregator(std::string_view local_node_id,
                    std::string_view peer_node_id, ClusterLabels cluster_labels)
      : SubscriptionProcessor(local_node_id, peer_node_id),
        cluster_labels(std::move(cluster_labels)) {
    for (const auto& [_key, value_pair] : this->cluster_labels) {
      if (value_pair.local_label != value_pair.remote_label) {
        same_cluster = false;
        break;
      }
    }
  }

  ~ClusterAggregator() = default;

  bool process_added(PubSub::Filter* filter) override {
    if (same_cluster) {
      return false;
    }

    for (const auto& [label_key, label_values] : cluster_labels) {
      if (!has_equality_constraint(filter, label_key,
                                   label_values.local_label)) {
        return false;
      }
    }

    if (published_count++ == 0) {
      std::vector<PubSub::Constraint> aggregate_constraints;
      for (const auto& [label_key, label_values] : cluster_labels) {
        aggregate_constraints.emplace_back(label_key, label_values.local_label);
      }
      *filter = PubSub::Filter(std::move(aggregate_constraints));
      return false;
    } else {
      return true;
    }
  }

  bool process_removed(PubSub::Filter* filter) override {
    if (same_cluster) {
      return false;
    }

    for (const auto& [label_key, label_values] : cluster_labels) {
      if (!has_equality_constraint(filter, label_key,
                                   label_values.local_label)) {
        return false;
      }
    }

    if (--published_count == 0) {
      std::vector<PubSub::Constraint> aggregate_constraints;
      for (const auto& [label_key, label_values] : cluster_labels) {
        aggregate_constraints.emplace_back(label_key, label_values.local_label);
      }
      *filter = PubSub::Filter(std::move(aggregate_constraints));
      return false;
    } else {
      return true;
    }
  }

 private:
  bool has_equality_constraint(PubSub::Filter* filter, std::string_view key,
                               std::string_view value) {
    for (const auto& constraint : filter->required) {
      if (constraint.attribute() == key &&
          std::get<std::string_view>(constraint.operand()) == value &&
          constraint.predicate() == PubSub::ConstraintPredicates::EQ) {
        return true;
      }
    }
    return false;
  }

  bool same_cluster = true;
  uint32_t published_count = 0;
  ClusterLabels cluster_labels;
};

}  // namespace uActor::Remote
