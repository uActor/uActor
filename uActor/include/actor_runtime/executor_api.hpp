#pragma once

#include <cstdint>

#include "pubsub/filter.hpp"
#include "pubsub/publication.hpp"
#include "pubsub/subscription_arguments.hpp"

namespace uActor::ActorRuntime {

struct ExecutorApi {
  virtual uint32_t add_subscription(
      uint32_t local_id, PubSub::Filter&& filter,
      PubSub::SubscriptionArguments arguments) = 0;
  virtual void remove_subscription(uint32_t local_id, uint32_t sub_id) = 0;
  virtual void delayed_publish(PubSub::Publication&& publication,
                               uint32_t delay) = 0;

  virtual void enqueue_wakeup(uint32_t actor_id, uint32_t delay,
                              std::string_view wakeup_id) = 0;
  virtual ~ExecutorApi() = default;
};

}  //  namespace uActor::ActorRuntime
