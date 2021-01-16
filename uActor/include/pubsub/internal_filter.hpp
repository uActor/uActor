#ifndef UACTOR_INCLUDE_PUBSUB_INTERNAL_FILTER_HPP_
#define UACTOR_INCLUDE_PUBSUB_INTERNAL_FILTER_HPP_

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "allocator_configuration.hpp"
#include "constraint.hpp"
#include "publication.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

// TODO(raphaelhetzel) there is much duplicate code between this and the regular
// filter class.
struct InternalFilter {
  template <typename U>
  using Allocator = RoutingAllocatorConfiguration::Allocator<U>;

  using allocator_type = Allocator<Filter>;

  using InternalConstraintList =
      std::vector<std::shared_ptr<const Constraint>,
                  Allocator<std::shared_ptr<const Constraint>>>;

  template <typename PAllocator = Allocator<InternalFilter>>
  explicit InternalFilter(const PAllocator& allocator = {})
      : required(allocator), optional(allocator), initialized(false) {}

  template <typename PAllocator = Allocator<InternalFilter>>
  InternalFilter(const InternalFilter& other, const PAllocator& allocator = {})
      : required(other.required, allocator),
        optional(other.optional, allocator),
        initialized(other.initialized) {}

  template <typename PAllocator = Allocator<InternalFilter>>
  InternalFilter(InternalFilter&& other, const PAllocator& allocator = {})
      : required(std::move(other.required), allocator),
        optional(std::move(other.optional), allocator),
        initialized(other.initialized) {
    other.initialized = false;
  }

  InternalFilter& operator=(const InternalFilter&& other) {
    required = other.required;
    optional = other.optional;
    initialized = other.initialized;
    return *this;
  }

  InternalFilter& operator=(InternalFilter&& other) {
    required = std::move(other.required);
    optional = std::move(other.optional);
    initialized = other.initialized;
    other.initialized = false;
    return *this;
  }

  bool matches(const Publication& publication) const {
    return check_required(publication) && check_optionals(publication);
  }

  bool check_required(const Publication& publication) const {
    for (auto constraint_ptr : required) {
      const auto& constraint = *constraint_ptr;
      auto attr = publication.get_attr(constraint.attribute());
      if (!std::holds_alternative<std::monostate>(attr)) {
        if (std::holds_alternative<std::string_view>(attr) &&
            !constraint(std::get<std::string_view>(attr))) {
          return false;
        }
        if (std::holds_alternative<int32_t>(attr) &&
            !constraint(std::get<int32_t>(attr))) {
          return false;
        }
        if (std::holds_alternative<float>(attr) &&
            !constraint(std::get<float>(attr))) {
          return false;
        }
      } else {
        return false;
      }
    }
    return true;
  }

  bool check_optionals(const Publication& publication) const {
    assert(initialized);
    if (optional.size() == 0) {
      return true;
    }
    for (const auto& constraint_ptr : optional) {
      assert(constraint_ptr);
      auto attr = publication.get_attr(constraint_ptr->attribute());
      if (!std::holds_alternative<std::monostate>(attr)) {
        if (std::holds_alternative<std::string_view>(attr) &&
            !(*constraint_ptr)(std::get<std::string_view>(attr))) {
          return false;
        }
        if (std::holds_alternative<int32_t>(attr) &&
            !(*constraint_ptr)(std::get<int32_t>(attr))) {
          return false;
        }
        if (std::holds_alternative<float>(attr) &&
            !(*constraint_ptr)(std::get<float>(attr))) {
          return false;
        }
      }
    }
    return true;
  }

  bool operator==(const Filter& other) const {
    if (required.size() != other.required.size() ||
        optional.size() != other.optional.size()) {
      return false;
    }
    for (auto c_ptr : required) {
      assert(initialized && c_ptr);
      if (std::find(other.required.begin(), other.required.end(), *c_ptr) ==
          other.required.end()) {
        return false;
      }
    }
    for (auto c_ptr : optional) {
      assert(initialized && c_ptr);
      if (std::find(other.optional.begin(), other.optional.end(), *c_ptr) ==
          other.optional.end()) {
        return false;
      }
    }
    return true;
  }

  bool operator==(const InternalFilter& other) const {
    if (required.size() != other.required.size() ||
        optional.size() != other.optional.size()) {
      return false;
    }
    for (auto c_ptr : required) {
      if (std::find_if(other.required.begin(), other.required.end(),
                       [c_ptr](const auto& item) { return *item == *c_ptr; }) ==
          other.required.end()) {
        return false;
      }
    }
    for (auto c_ptr : optional) {
      if (std::find_if(other.required.begin(), other.required.end(),
                       [c_ptr](const auto& item) { return *item == *c_ptr; }) ==
          other.optional.end()) {
        return false;
      }
    }
    return true;
  }

  std::string serialize() const {
    std::string serialized;
    for (auto constraint_ptr : required) {
      const auto& constraint = *constraint_ptr;
      serialized += constraint.serialize() + "^";
    }
    if (serialized.length() >= 2) {
      serialized.replace(serialized.length() - 1, 1, "?");
    }

    for (auto& constraint_ptr : optional) {
      const auto& constraint = *constraint_ptr;
      serialized += constraint.serialize() + "^";
    }

    if (serialized.length() >= 2) {
      serialized.resize(serialized.length() - 1);
    }
    return std::move(serialized);
  }

  // template <typename T>
  void set_constraints(InternalConstraintList&& required,
                       InternalConstraintList&& optional) {
    this->required = std::move(required);
    this->optional = std::move(optional);
    initialized = true;
  }

  InternalConstraintList required;
  InternalConstraintList optional;

  bool initialized;
};

}  // namespace uActor::PubSub

#endif  // UACTOR_INCLUDE_PUBSUB_INTERNAL_FILTER_HPP_
