#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>

#include "constraint_index.hpp"
#include "filter.hpp"
#include "publication.hpp"
#include "receiver.hpp"
#include "receiver_handle.hpp"
#include "subscription.hpp"
#include "support/allocator_config.hpp"
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

  std::string subscriptions_for(std::string_view node_id);

  void add_peer(std::string node_id);
  void remove_peer(std::string node_id);

  uint32_t find_sub_id(Filter&& filter);
  uint32_t number_of_subscriptions();

 private:
  using AllocatorConfiguration = uActor::Support::RoutingAllocatorConfiguration;

  template <typename U>
  using Allocator = AllocatorConfiguration::Allocator<U>;

  template <typename U>
  constexpr static auto make_allocator =
      AllocatorConfiguration::make_allocator<U>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  using ASubscription = Subscription<Allocator, AllocatorConfiguration>;
  using AConstraitIndex = ConstraintIndex<Allocator, AllocatorConfiguration>;

  std::atomic<uint32_t> next_sub_id{1};
  std::map<uint32_t, ASubscription, std::less<uint32_t>,
           Allocator<std::pair<const uint32_t, ASubscription>>>
      subscriptions{make_allocator<std::pair<const uint32_t, ASubscription>>()};
  std::map<AString, AConstraitIndex, Support::StringCMP,
           Allocator<std::pair<const AString, AConstraitIndex>>>
      constraints{make_allocator<std::pair<const AString, AConstraitIndex>>()};

  // Index for all subscriptions without required constraints.
  std::set<ASubscription*, std::less<ASubscription*>, Allocator<ASubscription*>>
      no_requirements{AllocatorConfiguration::make_allocator<ASubscription*>()};

  std::set<AString, Support::StringCMP, Allocator<AString>> node_id_sent_to{
      make_allocator<Allocator<AString>>()};
  std::set<AString, Support::StringCMP, Allocator<AString>> peer_node_ids{
      make_allocator<AString>()};

  std::atomic<bool> updated{false};

  std::shared_mutex mtx;

  uint32_t add_subscription(
      Filter&& f, Receiver* r,
      std::string subscriber_node_id = std::string("local"));
  uint32_t remove_subscription(uint32_t sub_id, Receiver* r,
                               std::string node_id = "");

  friend ReceiverHandle;
  friend Receiver;

  void publish_subscription_update();
  void publish_subscription_added(const Filter& filter, AString exclude,
                                  AString include);
  void publish_subscription_removed(const Filter& filter, AString exclude,
                                    AString include);
};

}  // namespace uActor::PubSub
