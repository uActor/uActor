#pragma once

#include "subscription_processor.hpp"

namespace uActor::Remote {
class ScopeLocal : public SubscriptionProcessor {
 public:
  ScopeLocal(std::string_view local_node_id, std::string_view peer_node_id)
      : SubscriptionProcessor(local_node_id, peer_node_id) {}

  ~ScopeLocal() = default;

  bool process_added(PubSub::Filter* filter,
                     const PubSub::SubscriptionArguments& arguments) override {
    if (arguments.scope == PubSub::Scope::LOCAL) {
      return true;
    }
    return false;
  }

  bool process_removed(
      PubSub::Filter* filter,
      const PubSub::SubscriptionArguments& arguments) override {
    if (arguments.scope == PubSub::Scope::LOCAL) {
      return true;
    }
    return false;
  }

 private:
};

}  // namespace uActor::Remote
