#pragma once
#include <string>

#include "pubsub/filter.hpp"

namespace uActor::Remote {

struct ForwarderSubscriptionAPI {
  virtual uint32_t add_remote_subscription(uint32_t local_id,
                                           PubSub::Filter&& filter,
                                           std::string node_id) = 0;
  virtual void remove_remote_subscription(uint32_t local_id, uint32_t sub_id,
                                          std::string node_id) = 0;

  virtual uint32_t add_local_subscription(uint32_t local_id,
                                          PubSub::Filter&& filter) = 0;
  virtual void remove_local_subscription(uint32_t local_id,
                                         uint32_t sub_id) = 0;

  virtual ~ForwarderSubscriptionAPI() {}
};

}  // namespace uActor::Remote
