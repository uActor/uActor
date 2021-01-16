#ifndef UACTOR_INCLUDE_PUBSUB_ALLOCATOR_CONFIGURATION_HPP_
#define UACTOR_INCLUDE_PUBSUB_ALLOCATOR_CONFIGURATION_HPP_

#include <memory>
#include <string>

#include "support/memory_manager.hpp"

namespace uActor::PubSub {

struct RoutingAllocatorConfiguration {
  template <typename U>
  using Allocator = Support::TrackingAllocator<U>;

  constexpr static auto allocator_params =
      std::tuple(Support::TrackedRegions::ROUTING_STATE);

  template <typename U>
  static Allocator<U> make_allocator() {
    return std::make_from_tuple<Allocator<U>>(allocator_params);
  }

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;
};

struct PublicationAllocatorConfiguration {
  template <typename U>
  using Allocator = Support::TrackingAllocator<U>;

  constexpr static auto allocator_params =
      std::tuple(Support::TrackedRegions::PUBLICATIONS);

  template <typename U>
  static Allocator<U> make_allocator() {
    return std::make_from_tuple<Allocator<U>>(allocator_params);
  }

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;
};
}  // namespace uActor::PubSub

#endif  // UACTOR_INCLUDE_PUBSUB_ALLOCATOR_CONFIGURATION_HPP_
