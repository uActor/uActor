#pragma once

#include <functional>
#include <list>
#include <map>
#include <string>
#include <utility>

#include "filter.hpp"
#include "receiver.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

template <template <typename> typename Allocator, typename AllocatorConfig>
struct Subscription {
  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  template <typename U>
  constexpr static auto make_allocator =
      AllocatorConfig::template make_allocator<U>;

  Subscription(uint32_t id, Filter f, AString node_id, Receiver* r)
      : subscription_id(id), filter(f) {
    nodes.emplace(
        node_id,
        receiver_list{{r},
                      make_allocator<typename receiver_list::value_type>()});
    receivers.emplace(
        r,
        node_list{{node_id}, make_allocator<typename node_list::value_type>()});
    count_required = filter.required.size();
    count_optional = filter.optional.size();
  }

  bool add_receiver(Receiver* receiver, AString source_node_id) {
    auto [receiver_it, inserted_receiver] = receivers.try_emplace(
        receiver, node_list{{source_node_id},
                            make_allocator<typename node_list::value_type>()});
    if (!inserted_receiver) {
      if (std::find(receiver_it->second.begin(), receiver_it->second.end(),
                    source_node_id) == receiver_it->second.end()) {
        receiver_it->second.push_back(source_node_id);
      }
    }

    auto [node_it, inserted_node] = nodes.try_emplace(
        source_node_id,
        receiver_list{{receiver},
                      make_allocator<typename node_list::value_type>()});
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

  std::pair<bool, size_t> remove_receiver(Receiver* receiver_ptr,
                                          AString source_node_id) {
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

  void remove_subscription_for_node(Receiver* r, AString node_id) {
    if (auto node_it = nodes.find(node_id); node_it != nodes.end()) {
      node_it->second.remove(r);
      if (node_it->second.size() == 0) {
        nodes.erase(node_it);
      }
    }
  }

  uint32_t subscription_id;
  Filter filter;
  size_t count_required, count_optional = 0;

  using receiver_list = std::list<Receiver*, Allocator<Receiver*>>;
  std::map<AString, receiver_list, Support::StringCMP,
           Allocator<std::pair<const AString, receiver_list>>>
      nodes{AllocatorConfig::template make_allocator<
          std::pair<const AString, receiver_list>>()};

  using node_list = std::list<AString, Allocator<AString>>;
  std::map<Receiver*, node_list, std::less<Receiver*>,
           Allocator<std::pair<Receiver* const, node_list>>>
      receivers{AllocatorConfig::template make_allocator<
          std::pair<Receiver* const, node_list>>()};
};

}  // namespace uActor::PubSub
