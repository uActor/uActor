#include "pubsub/subscription.hpp"

namespace uActor::PubSub {

Subscription::Subscription(uint32_t id, Filter f, std::string node_id,
                           Receiver* r)
    : subscription_id(id), filter(f) {
  nodes.emplace(node_id, std::list<Receiver*>{r});
  receivers.emplace(r, std::list<std::string>{node_id});
  count_required = filter.required.size();
  count_optional = filter.optional.size();
}

bool Subscription::add_receiver(Receiver* receiver,
                                std::string source_node_id) {
  auto [receiver_it, inserted_receiver] = receivers.try_emplace(
      receiver, std::list<std::string>{std::string(source_node_id)});
  if (!inserted_receiver) {
    if (std::find(receiver_it->second.begin(), receiver_it->second.end(),
                  source_node_id) == receiver_it->second.end()) {
      receiver_it->second.push_back(source_node_id);
    }
  }

  auto [node_it, inserted_node] =
      nodes.try_emplace(source_node_id, std::list<Receiver*>{receiver});
  if (!inserted_node) {
    if (std::find(node_it->second.begin(), node_it->second.end(), receiver) ==
        node_it->second.end()) {
      node_it->second.push_back(receiver);
    }
  } else {
    return true;
  }
  return false;
}

std::pair<bool, size_t> Subscription::remove_receiver(
    Receiver* receiver_ptr, std::string source_node_id) {
  if (auto receiver_it = receivers.find(receiver_ptr);
      receiver_it != receivers.end()) {
    if (source_node_id.empty()) {
      // Empty node id -> remove subscription for all
      // node_ids managed by the receiver
      for (auto& sub_node_id : receiver_it->second) {
        remove_subscription_for_node(receiver_ptr, sub_node_id);
      }
      receivers.erase(receiver_it);
      return std::make_pair(receivers.empty(), 0);
    } else {
      remove_subscription_for_node(receiver_ptr, source_node_id);
      receiver_it->second.remove(source_node_id);
      if (receiver_it->second.empty()) {
        receivers.erase(receiver_it);
        return std::make_pair(receivers.empty(), 0);
      } else {
        return std::make_pair(false, receiver_it->second.size());
      }
    }
  } else {
    return std::make_pair(receivers.empty(), 0);
  }
}

void Subscription::remove_subscription_for_node(Receiver* r,
                                                std::string node_id) {
  if (auto node_it = nodes.find(node_id); node_it != nodes.end()) {
    node_it->second.remove(r);
    if (node_it->second.size() == 0) {
      nodes.erase(node_it);
    }
  }
}

}  // namespace uActor::PubSub
