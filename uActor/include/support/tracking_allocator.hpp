#ifndef UACTOR_INCLUDE_SUPPORT_TRACKING_ALLOCATOR_HPP_
#define UACTOR_INCLUDE_SUPPORT_TRACKING_ALLOCATOR_HPP_

#include <functional>
#include <memory>

#include "memory_manager.hpp"

namespace uActor::Support {

template <typename T>
struct TrackingAllocator {
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;

  TrackedRegions region{TrackedRegions::GENERAL};

  std::allocator<T> base = std::allocator<T>();

  TrackingAllocator() noexcept {}
  TrackingAllocator(const TrackingAllocator&) noexcept = default;
  ~TrackingAllocator() = default;

  explicit TrackingAllocator(TrackedRegions region) : region(region) {}

  template <class U>
  TrackingAllocator(const TrackingAllocator<U>& other) noexcept
      : region(other.region) {}

  constexpr T* allocate(std::size_t n) {
    MemoryManager::total_space[static_cast<size_t>(region)] += n * sizeof(T);
    if (MemoryManager::total_space[static_cast<size_t>(region)] >
        MemoryManager::max_space[static_cast<size_t>(region)]) {
      MemoryManager::max_space[static_cast<size_t>(region)] =
          MemoryManager::total_space[static_cast<size_t>(region)];
    }
    return base.allocate(n);
  }

  constexpr void deallocate(T* p, std::size_t n) {
    MemoryManager::total_space[static_cast<size_t>(region)] -= n * sizeof(T);
    base.deallocate(p, n);
  }
};

template <class T, class U>
bool operator==(const uActor::Support::TrackingAllocator<T>& a_t,
                const uActor::Support::TrackingAllocator<U>& a_u) {
  return true;
}

template <class T, class U>
bool operator!=(const uActor::Support::TrackingAllocator<T>& a_t,
                const uActor::Support::TrackingAllocator<U>& a_u) {
  return false;
}

}  // namespace uActor::Support

#endif  // UACTOR_INCLUDE_SUPPORT_TRACKING_ALLOCATOR_HPP_
