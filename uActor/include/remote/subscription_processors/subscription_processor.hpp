#pragma once

#include <string_view>

#include "pubsub/filter.hpp"

namespace uActor::Remote {

class SubscriptionProcessor {
 public:
  SubscriptionProcessor(std::string_view local_node_id,
                        std::string_view peer_node_id)
      : local_node_id(local_node_id), peer_node_id(peer_node_id) {}

  virtual bool process_added(PubSub::Filter* filter) { return false; }

  virtual bool process_removed(PubSub::Filter* filter) { return false; }

  virtual ~SubscriptionProcessor() = default;

 protected:
  std::string_view local_node_id;
  std::string_view peer_node_id;
};

}  // namespace uActor::Remote
