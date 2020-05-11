#ifndef UACTOR_INCLUDE_REMOTE_FORWARDER_API_HPP_
#define UACTOR_INCLUDE_REMOTE_FORWARDER_API_HPP_

#include "pubsub/filter.hpp"

namespace uActor::Remote {

struct ForwarderSubscriptionAPI {
  virtual uint32_t add_subscription(uint32_t local_id, PubSub::Filter&& filter,
                                    std::string_view node_id) = 0;
  virtual void remove_subscription(uint32_t local_id, uint32_t sub_id,
                                   std::string_view node_id) = 0;
  virtual bool write(int sock, int len, const char* message) = 0;
  virtual ~ForwarderSubscriptionAPI() {}
};

}  // namespace uActor::Remote

#endif  //  UACTOR_INCLUDE_REMOTE_FORWARDER_API_HPP_
