#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <scoped_allocator>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "allocator_configuration.hpp"
#include "constraint_index.hpp"
#include "filter.hpp"
#include "publication.hpp"
#include "receiver.hpp"
#include "receiver_handle.hpp"
#include "subscription.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

class Router {
 public:
  static Router& get_instance() {
    static Router instance;
    return instance;
  }

  void publish(Publication&& publication);
  void publish_internal(Publication&& publication);

  ReceiverHandle new_subscriber();

  std::vector<std::string> subscriptions_for(std::string_view node_id,
                                             uint32_t max_size = 0);

  uint32_t find_sub_id(Filter&& filter);
  uint32_t number_of_subscriptions();

 private:
  template <typename U>
  using Allocator = RoutingAllocatorConfiguration::Allocator<U>;

  template <typename U>
  constexpr static auto make_allocator =
      RoutingAllocatorConfiguration::make_allocator<U>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  using ASubscription = Subscription<Allocator>;
  using AConstraitIndex = ConstraintIndex<Allocator>;

  std::atomic<uint32_t> next_sub_id{1};

  std::map<uint32_t, ASubscription, std::less<uint32_t>,
           std::scoped_allocator_adaptor<
               Allocator<std::pair<const uint32_t, ASubscription>>>>
      subscriptions{make_allocator<std::pair<const uint32_t, ASubscription>>()};

  std::map<AString, AConstraitIndex, Support::StringCMP,
           std::scoped_allocator_adaptor<
               Allocator<std::pair<const AString, AConstraitIndex>>>>
      constraints{make_allocator<std::pair<const uint32_t, ASubscription>>()};

  // Index for all subscriptions without required constraints.
  std::set<ASubscription*, std::less<ASubscription*>, Allocator<ASubscription*>>
      no_requirements{make_allocator<ASubscription*>()};

  // TODO(raphaelhetzel): This is not cleaned up when a peer is removed.
  std::map<AString, std::pair<bool, uint32_t>, Support::StringCMP,
           Allocator<std::pair<const AString, std::pair<bool, uint32_t>>>>
      peer_node_ids{{{AString("local"), {true, 1}}},
                    make_allocator<
                        std::pair<const AString, std::pair<bool, uint32_t>>>()};

  std::atomic<bool> updated{false};

  std::shared_mutex mtx;

  std::atomic<uint32_t> next_node_id{2};

  uint32_t add_subscription(
      Filter&& f, Receiver* r,
      std::string subscriber_node_id = std::string("local"));

  uint32_t remove_subscription(uint32_t sub_id, Receiver* r,
                               std::string node_id = "");

  friend ReceiverHandle;
  friend Receiver;

  void publish_subscription_added(const Filter& filter, AString exclude,
                                  AString include);
  void publish_subscription_removed(const InternalFilter& filter,
                                    AString exclude, AString include);
};

}  // namespace uActor::PubSub
