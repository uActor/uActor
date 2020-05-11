#ifndef UACTOR_INCLUDE_PUBSUB_SUBSCRIPTION_HPP_
#define UACTOR_INCLUDE_PUBSUB_SUBSCRIPTION_HPP_

#include <list>
#include <map>
#include <string>
#include <utility>

#include "filter.hpp"
#include "receiver.hpp"

namespace uActor::PubSub {

struct Subscription {
  Subscription(uint32_t id, Filter f, std::string node_id, Receiver* r);

  bool add_receiver(Receiver* receiver, std::string source_node_id);

  std::pair<bool, size_t> remove_receiver(Receiver* receiver_ptr,
                                          std::string source_node_id);

  void remove_subscription_for_node(Receiver* r, std::string node_id);

  uint32_t subscription_id;
  Filter filter;
  size_t count_required, count_optional = 0;

  std::map<std::string, std::list<Receiver*>> nodes;
  std::map<Receiver*, std::list<std::string>> receivers;
};

}  // namespace uActor::PubSub

#endif  //  UACTOR_INCLUDE_PUBSUB_SUBSCRIPTION_HPP_
