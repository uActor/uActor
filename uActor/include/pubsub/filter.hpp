#pragma once

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <scoped_allocator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "allocator_configuration.hpp"
#include "constraint.hpp"
#include "publication.hpp"

namespace uActor::Remote {
class SubscriptionFilter;
class NodeLocalFilterDrop;
class NodeIdAggregator;
class ClusterBarrier;
class ClusterAggregator;
class OptionalConstraintDrop;
class LeafNodeIdGateway;
class ScopeLocal;
class ScopeCluster;
};  // namespace uActor::Remote

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

  [[nodiscard]] bool operator!=(const Filter& other) const {
    return !operator==(other);
  }

  [[nodiscard]] const std::vector<
      Constraint, std::scoped_allocator_adaptor<Allocator<Constraint>>>&
  required_constraints() const {
    return required;
  }

  [[nodiscard]] const std::vector<
      Constraint, std::scoped_allocator_adaptor<Allocator<Constraint>>>&
  optional_constraints() const {
    return optional;
  }

  void add_constraint_to_map(const Constraint& constraint,
                             Publication::Map* map) const;

  std::shared_ptr<PubSub::Publication::Map> to_publication_map() const;

  static std::optional<Constraint> parse_constraint_from_map(
      std::string_view key, const Publication::Map& item);

  static std::optional<Filter> from_publication_map(
      const Publication::Map& map);

 private:
  std::vector<Constraint, std::scoped_allocator_adaptor<Allocator<Constraint>>>
      required;
  std::vector<Constraint, std::scoped_allocator_adaptor<Allocator<Constraint>>>
      optional;

  template <template <typename> typename Allocator>
  friend class Subscription;
  friend class InternalFilter;
  friend class Remote::SubscriptionFilter;
  friend class Remote::NodeLocalFilterDrop;
  friend class Remote::NodeIdAggregator;
  friend class Remote::ClusterBarrier;
  friend class Remote::ClusterAggregator;
  friend class Remote::OptionalConstraintDrop;
  friend class Remote::LeafNodeIdGateway;
  friend class Remote::ScopeLocal;
  friend class Remote::ScopeCluster;
  friend Router;
};
}  // namespace uActor::PubSub
