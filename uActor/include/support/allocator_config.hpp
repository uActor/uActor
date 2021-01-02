#ifndef UACTOR_INCLUDE_SUPPORT_ALLOCATOR_CONFIG_HPP_
#define UACTOR_INCLUDE_SUPPORT_ALLOCATOR_CONFIG_HPP_

#include <memory>

#include "support/memory_manager.hpp"

namespace uActor::Support {
struct RoutingAllocatorConfiguration {
  template <typename U>
  using Allocator = TrackingAllocator<U>;

  constexpr static auto allocator_params =
      std::tuple(TrackedRegions::ROUTING_STATE);

  template <typename U>
  static Allocator<U> make_allocator() {
    return std::make_from_tuple<Allocator<U>>(allocator_params);
  }
};

struct PublicationAllocatorConfiguration {
  template <typename U>
  using Allocator = TrackingAllocator<U>;

  constexpr static auto allocator_params =
      std::tuple(TrackedRegions::PUBLICATIONS);

  template <typename U>
  static Allocator<U> make_allocator() {
    return std::make_from_tuple<Allocator<U>>(allocator_params);
  }
};

struct DefaultRuntimeAllocatorConfiguration {
  template <typename U>
  using Allocator = TrackingAllocator<U>;

  constexpr static auto allocator_params =
      std::tuple(TrackedRegions::ACTOR_RUNTIME);

  template <typename U>
  static Allocator<U> make_allocator() {
    return std::make_from_tuple<Allocator<U>>(allocator_params);
  }
};
}  // namespace uActor::Support

#endif  // UACTOR_INCLUDE_SUPPORT_ALLOCATOR_CONFIG_HPP_
