#pragma once

#include "subscription_processor.hpp"

namespace uActor::Remote {
class ScopeCluster : public SubscriptionProcessor {
 public:
  ScopeCluster(std::string_view local_node_id, std::string_view peer_node_id,
               std::string_view local_cluster, std::string_view peer_cluster)
      : SubscriptionProcessor(local_node_id, peer_node_id) {
    other_cluster = local_cluster != peer_cluster;
  }

  ~ScopeCluster() = default;

  bool process_added(PubSub::Filter* filter,
                     const PubSub::SubscriptionArguments& arguments) override {
    if (arguments.scope == PubSub::Scope::CLUSTER && other_cluster) {
      return true;
    }
    return false;
  }

  bool process_removed(
      PubSub::Filter* filter,
      const PubSub::SubscriptionArguments& arguments) override {
    if (arguments.scope == PubSub::Scope::CLUSTER && other_cluster) {
      return true;
    }
    return false;
  }

 private:
  bool other_cluster = false;
};

}  // namespace uActor::Remote
