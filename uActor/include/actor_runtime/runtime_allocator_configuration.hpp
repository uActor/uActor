#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_RUNTIME_ALLOCATOR_CONFIGURATION_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_RUNTIME_ALLOCATOR_CONFIGURATION_HPP_

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif

#include <memory>
#include <tuple>

#include "support/tracking_allocator.hpp"

namespace uActor::ActorRuntime {

struct RuntimeAllocatorConfiguration {
#if CONFIG_UACTOR_ENABLE_HEAP_TRACKING
  template <typename U>
  using Allocator = Support::TrackingAllocator<U>;

  constexpr static auto allocator_params =
      std::tuple(Support::TrackedRegions::ACTOR_RUNTIME);
#else
  template <typename U>
  using Allocator = std::allocator<U>;

  constexpr static auto allocator_params = std::tuple<>();
#endif

  template <typename U>
  static Allocator<U> make_allocator() {
    return std::make_from_tuple<Allocator<U>>(allocator_params);
  }
};

}  // namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_RUNTIME_ALLOCATOR_CONFIGURATION_HPP_
