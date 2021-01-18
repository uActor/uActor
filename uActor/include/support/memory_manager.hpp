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
#if CONFIG_UACTOR_ENABLE_HEAP_TRACKING
    int32_t size_difference = static_cast<int32_t>(nsize);
    if (ptr) {
      size_difference -= static_cast<int32_t>(osize);
    }

    total_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)] +=
        size_difference;
    if (total_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)] >
        max_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)]) {
      max_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)] =
          total_space[static_cast<size_t>(TrackedRegions::ACTOR_RUNTIME)];
    }
#endif

    // standard lua allocator
    if (nsize == 0) {
      free(ptr);
      return NULL;
    } else {
      return realloc(ptr, nsize);
    }
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

};      // namespace uActor::Support
#endif  // UACTOR_INCLUDE_SUPPORT_MEMORY_MANAGER_HPP_
