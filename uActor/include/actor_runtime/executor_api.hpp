#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_EXECUTOR_API_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_EXECUTOR_API_HPP_

#include <cstdint>

#include "pubsub/filter.hpp"
#include "pubsub/publication.hpp"

namespace uActor::ActorRuntime {

struct ExecutorApi {
  virtual uint32_t add_subscription(uint32_t local_id,
                                    PubSub::Filter&& filter) = 0;
  virtual void remove_subscription(uint32_t local_id, uint32_t sub_id) = 0;
  virtual void delayed_publish(PubSub::Publication&& publication,
                               uint32_t delay) = 0;
  virtual ~ExecutorApi() {}
};

}  //  namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_EXECUTOR_API_HPP_
