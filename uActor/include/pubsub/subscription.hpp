#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <scoped_allocator>
#include <set>
#include <string>
#include <utility>

#include "filter.hpp"
#include "internal_filter.hpp"
#include "receiver.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

template <template <typename> typename Allocator>
struct Subscription {
  using allocator_type = Allocator<Subscription>;

  template <typename PAllocator = Allocator<Subscription>>
  Subscription(std::allocator_arg_t, const PAllocator& allocator, uint32_t id,
               Receiver* r, uint32_t node_id, uint8_t priority = 0)
      : subscription_id(id),
        filter(allocator),
        priority(priority),
        receivers(allocator) {
    receivers.emplace(r, std::initializer_list<uint32_t>{node_id});
  }

  template <typename PAllocator = Allocator<Subscription>>
  Subscription(std::allocator_arg_t, const PAllocator& allocator,
               const Subscription& other)
      : subscription_id(other.subscription_id),
        filter(other.filter, allocator),
        priority(other.priority),
        receivers(other.receivers, allocator) {}

  template <typename PAllocator = Allocator<Subscription>>
  Subscription(std::allocator_arg_t, const PAllocator& allocator,
               Subscription&& other)
      : subscription_id(other.subscription_id),
        filter(std::move(other.filter), allocator),
        priority(other.priority),
        receivers(std::move(other.receivers), allocator) {}

  bool add_receiver(Receiver* receiver, uint32_t source_node_id) {
    auto [receiver_it, inserted_receiver] = receivers.try_emplace(receiver);
    auto [it, inserted] = receiver_it->second.emplace(source_node_id);

    if (inserted) {
      for (const auto& r : receivers) {
        for (uint32_t n : r.second) {
          if (n == source_node_id) {
            return false;
          }
        }
      }
      return true;
    }
    return false;
  }

  template <typename T>
  void set_constraints(T&& required, T&& optional) {
    filter.set_constraints(std::move(required), std::move(optional));
  }

  std::set<uint32_t> nodes() {
    std::set<uint32_t> temp_nodes;
    for (const auto& r : receivers) {
      for (uint32_t n : r.second) {
        temp_nodes.emplace(n);
      }
    }
    return temp_nodes;
  }

  std::pair<bool, size_t> remove_receiver(Receiver* receiver_ptr,
                                          uint32_t source_node_id) {
    if (auto receiver_it = receivers.find(receiver_ptr);
        receiver_it != receivers.end()) {
      if (source_node_id == 0) {
        // Empty node id -> remove subscription for all
        // node_ids managed by the receiver
        receivers.erase(receiver_it);
        return std::make_pair(receivers.empty(), 0);
      } else {
        receiver_it->second.erase(source_node_id);
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

  uint32_t subscription_id;

  InternalFilter filter;

  uint32_t count_required() { return filter.required.size(); }

  uint32_t count_optional() { return filter.optional.size(); }

  using NodeList = std::set<uint32_t, std::less<uint32_t>, Allocator<uint32_t>>;
  uint8_t priority;

  std::map<Receiver*, NodeList, std::less<Receiver*>,
           std::scoped_allocator_adaptor<
               Allocator<std::pair<Receiver* const, NodeList>>>>
      receivers;
};

}  // namespace uActor::PubSub
