#ifndef MAIN_INCLUDE_RUNTIME_API_HPP_
#define MAIN_INCLUDE_RUNTIME_API_HPP_

#include <cstdint>

#include "publication.hpp"
#include "subscription.hpp"

struct RuntimeApi {
  virtual uint32_t add_subscription(uint32_t local_id,
                                    PubSub::Filter&& filter) = 0;
  virtual void remove_subscription(uint32_t local_id, uint32_t sub_id) = 0;
  virtual void delayed_publish(Publication&& publication, uint32_t delay) = 0;
  virtual ~RuntimeApi() {}
};

#endif  // MAIN_INCLUDE_RUNTIME_API_HPP_
