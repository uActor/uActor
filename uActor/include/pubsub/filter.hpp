#pragma once

#include <functional>
#include <list>
#include <optional>
#include <scoped_allocator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "allocator_configuration.hpp"
#include "constraint.hpp"
#include "publication.hpp"

namespace uActor::PubSub {

template <template <typename> typename Allocator>
class Subscription;

class InternalFilter;

class Router;

class Filter {
 public:
  template <typename U>
  using Allocator = RoutingAllocatorConfiguration::Allocator<U>;

  using allocator_type = Allocator<Filter>;

  template <typename PAllocator = allocator_type>
  explicit Filter(std::initializer_list<Constraint> constraints,
                  const PAllocator& allocator =
                      RoutingAllocatorConfiguration::make_allocator<Filter>())
      : required(allocator), optional(allocator) {
    for (Constraint c : constraints) {
      if (c.optional()) {
        optional.emplace_back(std::move(c));
      } else {
        required.emplace_back(std::move(c));
      }
    }
  }

  template <typename PAllocator = allocator_type, typename T>
  explicit Filter(T constraints,
                  const PAllocator& allocator =
                      RoutingAllocatorConfiguration::make_allocator<Filter>())
      : required(allocator), optional(allocator) {
    for (Constraint c : constraints) {
      if (c.optional()) {
        optional.push_back(std::move(c));
      } else {
        required.push_back(std::move(c));
      }
    }
  }

  template <typename PAllocator = allocator_type>
  Filter(const Filter& other,
         const PAllocator& allocator =
             RoutingAllocatorConfiguration::make_allocator<Filter>())
      : required(other.required, allocator),
        optional(other.optional, allocator) {}

  template <typename PAllocator = allocator_type>
  Filter(Filter&& other, const PAllocator& allocator = {})
      : required(std::move(other.required), allocator),
        optional(std::move(other.optional), allocator) {}

  //  template<typename PAllocator = allocator_type>
  //  explicit Filter(std::allocator_arg_t, const PAllocator& allocator,
  //  std::list<Constraint>&& constraints) {
  //
  //  }

  Filter() = default;

  void clear();

  [[nodiscard]] bool matches(const Publication& publication) const;

  [[nodiscard]] bool check_required(const Publication& publication) const;

  [[nodiscard]] bool check_optionals(const Publication& publication) const;

  [[nodiscard]] bool operator==(const Filter& other) const;

  [[nodiscard]] const std::vector<Constraint,
                    std::scoped_allocator_adaptor<Allocator<Constraint>>>&
  required_constraints() const {
    return required;
  }

  [[nodiscard]] const std::vector<Constraint,
                    std::scoped_allocator_adaptor<Allocator<Constraint>>>&
  optional_constraints() const {
    return optional;
  }

  [[nodiscard]] std::string serialize() const;

  static std::optional<Filter> deserialize(std::string_view serialized);

 private:
  std::vector<Constraint, std::scoped_allocator_adaptor<Allocator<Constraint>>>
      required;
  std::vector<Constraint, std::scoped_allocator_adaptor<Allocator<Constraint>>>
      optional;

  template <template <typename> typename Allocator>
  friend class Subscription;
  friend class InternalFilter;
  friend Router;
};
}  // namespace uActor::PubSub
