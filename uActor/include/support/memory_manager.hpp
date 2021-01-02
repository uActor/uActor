#ifndef UACTOR_INCLUDE_SUPPORT_MEMORY_MANAGER_HPP_
#define UACTOR_INCLUDE_SUPPORT_MEMORY_MANAGER_HPP_

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string_view>

namespace uActor::Support {

enum class TrackedRegions : size_t {
  GENERAL = 0,
  ACTOR_RUNTIME,
  ROUTING_STATE,
  PUBLICATIONS,
  DEBUG,
  _TAIL
};

struct MemoryManager {
  static int32_t total_space[static_cast<size_t>(TrackedRegions::_TAIL)];
  static int32_t max_space[static_cast<size_t>(TrackedRegions::_TAIL)];

  static void* allocate_lua(void* ud, void* ptr, size_t osize, size_t nsize) {
    total_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)] +=
        static_cast<int32_t>(nsize) - static_cast<int32_t>(osize);
    if (total_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)] >
        max_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)]) {
      max_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)] =
          total_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)];
    }

    // standard lua allocator
    if (nsize == 0) {
      free(ptr);
      return NULL;
    } else {
      return realloc(ptr, nsize);
    }
  }
};

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

  explicit TrackingAllocator(TrackedRegions region) : region(region){}

  template <class U>
  TrackingAllocator(const TrackingAllocator<U>& other) noexcept
      : region(other.region) {}

  constexpr T* allocate(std::size_t n) {
    if (region == TrackedRegions::GENERAL) {
      printf("a");
    }
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

struct StringCMP {
  using is_transparent = void;
  template <typename T, typename U>
  constexpr auto operator()(const T& l, const U& r) const {
    return std::less<std::string_view>()(std::string_view(l),
                                         std::string_view(r));
  }
};

struct StringEqual {
  using is_transparent = void;
  template <typename T, typename U>
  constexpr auto operator()(const T& l, const U& r) const {
    return std::equal_to<std::string_view>()(std::string_view(l),
                                             std::string_view(r));
  }
};

struct StringHash {
  using is_transparent = void;
  template <typename T>
  constexpr auto operator()(const T& l) const {
    return std::hash<std::string_view>()(std::string_view(l));
  }
};

};  // namespace uActor::Support

template <typename T, typename U>
constexpr bool operator==(
    const uActor::Support::TrackingAllocator<T>&,
    const uActor::Support::TrackingAllocator<U>&) noexcept {
  return true;
}

template <typename T, typename U>
constexpr bool operator!=(
    const uActor::Support::TrackingAllocator<T>&,
    const uActor::Support::TrackingAllocator<U>&) noexcept {
  return false;
}

#endif  // UACTOR_INCLUDE_SUPPORT_MEMORY_MANAGER_HPP_
