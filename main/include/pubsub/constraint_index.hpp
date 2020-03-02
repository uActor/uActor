#ifndef MAIN_INCLUDE_PUBSUB_CONSTRAINT_INDEX_HPP_
#define MAIN_INCLUDE_PUBSUB_CONSTRAINT_INDEX_HPP_

#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

#include "constraint.hpp"
#include "subscription.hpp"

namespace uActor::PubSub {

struct SubscriptionCounter {
  size_t required = 0;
  size_t optional = 0;
};

using Counter = std::unordered_map<Subscription*, SubscriptionCounter>;

struct ConstraintIndex {
  // optimized index for some cases
  std::map<std::string, std::map<Subscription*, bool>> string_equal;

  // Fallback to linar scan for all other types of constraints
  std::list<std::pair<Constraint, std::map<Subscription*, bool>>> data;

  void insert(Constraint c, Subscription* sub_ptr, bool opt);

  bool remove(const Constraint& c, Subscription* sub_ptr);

  void check(std::variant<std::string, int32_t, float> value, Counter* counter);
};

}  //  namespace uActor::PubSub

#endif  //   MAIN_INCLUDE_PUBSUB_CONSTRAINT_INDEX_HPP_
