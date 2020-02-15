#ifndef MAIN_INCLUDE_PUBSUB_SUBSCRIPTION_HPP_
#define MAIN_INCLUDE_PUBSUB_SUBSCRIPTION_HPP_

#include <list>
#include <string>
#include <unordered_map>

#include "filter.hpp"
#include "receiver.hpp"

namespace uActor::PubSub {

struct Subscription {
  Subscription(uint32_t id, Filter f, std::string node_id, Receiver* r)
      : subscription_id(id), filter(f) {
    nodes.emplace(node_id, std::list<Receiver*>{r});
    receivers.emplace(r, std::list<std::string>{node_id});
    count_required = filter.required.size();
    count_optional = filter.optional.size();
  }

  uint32_t subscription_id;
  Filter filter;
  size_t count_required, count_optional = 0;

  std::unordered_map<std::string, std::list<Receiver*>> nodes;
  std::unordered_map<Receiver*, std::list<std::string>> receivers;
};

}  // namespace uActor::PubSub

#endif  //  MAIN_INCLUDE_PUBSUB_SUBSCRIPTION_HPP_
