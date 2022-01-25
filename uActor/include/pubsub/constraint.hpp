#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "allocator_configuration.hpp"
#include "publication.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

struct ConstraintPredicates {
  constexpr static const uint32_t MAX_INDEX = 10;
  enum Predicate : uint32_t { EQ = 1, NE, LT, GT, GE, LE, PF, SF, CT, MAP_ALL };

  static const char* name(uint32_t tag);
  static std::optional<Predicate> from_string(std::string_view name);
};

class Constraint {
  template <typename U>
  using Allocator = RoutingAllocatorConfiguration::Allocator<U>;

  using allocator_type = Allocator<Constraint>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  template <typename T>
  struct Container {
    Container(T operand, ConstraintPredicates::Predicate operation_name)
        : operand(std::move(operand)), operation_name(operation_name) {}

    T operand;
    ConstraintPredicates::Predicate operation_name;

    template <typename I = T>
    [[nodiscard]] bool match(I input) const {
      switch (operation_name) {
        case ConstraintPredicates::Predicate::EQ:
          return std::equal_to<I>()(input, operand);
        case ConstraintPredicates::Predicate::LT:
          return std::less<I>()(input, operand);
        case ConstraintPredicates::Predicate::GT:
          return std::greater<I>()(input, operand);
        case ConstraintPredicates::Predicate::GE:
          return std::greater_equal<I>()(input, operand);
        case ConstraintPredicates::Predicate::LE:
          return std::less_equal<I>()(input, operand);
        case ConstraintPredicates::Predicate::NE:
          return std::not_equal_to<I>()(input, operand);
        default:  // We ignore constraints that are not type-idenpendent.
          return false;
      }
    }

    bool operator==(const Container<T>& other) const {
      return operand == other.operand && operation_name == other.operation_name;
    }

    bool operator<(const Container<T>& other) const {
      return operation_name < other.operation_name ||
             (operation_name == other.operation_name &&
              operand < other.operand);
    }
  };

 public:
  // TODO(raphaelhetzel) combine constructors once we use C++20

  template <typename PAllocator = allocator_type>
  Constraint(
      std::string attribute, std::string_view oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false,
      const PAllocator& allocator =
          RoutingAllocatorConfiguration::make_allocator<Constraint>())
      : _attribute(attribute, allocator),
        _operand(Container<AString>(AString(oper, allocator), op)),
        _optional(optional) {
    if (_attribute.at(0) == '?') {
      _attribute = _attribute.substr(1);
      _optional = true;
    }
  }

  template <typename PAllocator = allocator_type>
  Constraint(
      std::string attribute, int32_t oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false,
      const PAllocator& allocator =
          RoutingAllocatorConfiguration::make_allocator<Constraint>())
      : _attribute(attribute, allocator),
        _operand(Container<int32_t>(oper, op)),
        _optional(optional) {
    if (_attribute.at(0) == '?') {
      _attribute = _attribute.substr(1);
      _optional = true;
    }
  }

  template <typename PAllocator = allocator_type>
  Constraint(
      std::string attribute, float oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false,
      const PAllocator& allocator =
          RoutingAllocatorConfiguration::make_allocator<Constraint>())
      : _attribute(attribute, allocator),
        _operand(Container<float>(oper, op)),
        _optional(optional) {
    if (_attribute.at(0) == '?') {
      _attribute = _attribute.substr(1);
      _optional = true;
    }
  }

  template <typename PAllocator = allocator_type>
  Constraint(std::string attribute, std::vector<Constraint>&& oper,
             ConstraintPredicates::Predicate op =
                 ConstraintPredicates::Predicate::MAP_ALL,
             bool optional = false,
             const PAllocator& allocator =
                 RoutingAllocatorConfiguration::make_allocator<Constraint>())
      : _attribute(attribute, allocator),
        _operand(Container<std::vector<Constraint>>(std::move(oper), op)),
        _optional(optional) {
    if (_attribute.at(0) == '?') {
      _attribute = _attribute.substr(1);
      _optional = true;
    }
  }

  template <typename PAllocator = allocator_type>
  Constraint(Constraint&& other,
             PAllocator allocator =
                 RoutingAllocatorConfiguration::make_allocator<Constraint>())
      : _attribute(std::move(other._attribute), allocator),
        _optional(other._optional) {
    if (std::holds_alternative<Container<int32_t>>(other._operand) ||
        std::holds_alternative<Container<float>>(other._operand)) {
      _operand = std::move(other._operand);
    } else if (std::holds_alternative<Container<AString>>(other._operand)) {
      auto&& o = std::get<Container<AString>>(other._operand);
      _operand = Container<AString>(AString(std::move(o.operand), allocator),
                                    o.operation_name);
    }
  }

  template <typename PAllocator = allocator_type>
  Constraint(const Constraint& other,
             PAllocator allocator =
                 RoutingAllocatorConfiguration::make_allocator<Constraint>())
      : _attribute(other._attribute, allocator), _optional(other._optional) {
    if (std::holds_alternative<Container<int32_t>>(other._operand) ||
        std::holds_alternative<Container<float>>(other._operand)) {
      _operand = other._operand;
    } else if (std::holds_alternative<Container<AString>>(other._operand)) {
      auto&& o = std::get<Container<AString>>(other._operand);
      _operand =
          Container<AString>(AString(o.operand, allocator), o.operation_name);
    }
  }

  bool operator()(std::string_view input) const;

  bool operator()(int32_t input) const;

  bool operator()(float input) const;

  bool operator()(const Publication::Map& input) const;

  bool operator==(const Constraint& other) const;

  bool operator<(const Constraint& other) const;

  [[nodiscard]] bool optional() const { return _optional; }

  [[nodiscard]] const std::string_view attribute() const {
    return std::string_view(_attribute);
  }

  [[nodiscard]] std::string serialize() const;

  static std::optional<Constraint> deserialize(std::string_view serialized,
                                               bool optional);

  [[nodiscard]] std::variant<std::monostate, std::string_view, int32_t, float,
                             const std::vector<Constraint>*>
  operand() const;

  [[nodiscard]] ConstraintPredicates::Predicate predicate() const;

 private:
  AString _attribute;
  std::variant<std::monostate, Container<AString>, Container<int32_t>,
               Container<float>, Container<std::vector<Constraint>>>
      _operand;
  bool _optional;
};

// Add Prefix, Suffix, and Contains operators to the string constraints.
template <>
template <>
[[nodiscard]] inline bool
Constraint::Container<Constraint::AString>::match<Constraint::AString>(
    AString input) const {
  switch (operation_name) {
    case ConstraintPredicates::Predicate::EQ:
      return std::equal_to<AString>()(input, operand);
    case ConstraintPredicates::Predicate::LT:
      return std::less<AString>()(input, operand);
    case ConstraintPredicates::Predicate::GT:
      return std::greater<AString>()(input, operand);
    case ConstraintPredicates::Predicate::GE:
      return std::greater_equal<AString>()(input, operand);
    case ConstraintPredicates::Predicate::LE:
      return std::less_equal<AString>()(input, operand);
    case ConstraintPredicates::Predicate::NE:
      return std::not_equal_to<AString>()(input, operand);
    case ConstraintPredicates::Predicate::PF:
      // Adapted from https://stackoverflow.com/a/42844629
      return input.size() >= operand.size() &&
             0 == input.compare(0, operand.size(), operand);
    case ConstraintPredicates::Predicate::SF: {
      // Adapted from https://stackoverflow.com/a/42844629
      return input.size() >= operand.size() &&
             0 == input.compare(input.size() - operand.size(), operand.size(),
                                operand);
    }
    case ConstraintPredicates::Predicate::CT:
      return input.find(operand) != std::string::npos;
    default:
      return false;
  }
  return false;
}

// Enable the nested constraint check for map constraints.
template <>
template <>
[[nodiscard]] inline bool Constraint::Container<std::vector<Constraint>>::match<
    const PubSub::Publication::Map&>(
    const PubSub::Publication::Map& input) const {
  switch (operation_name) {
    case ConstraintPredicates::Predicate::MAP_ALL:
      for (const auto& constraint : operand) {
        if (std::holds_alternative<std::string_view>(constraint.operand())) {
          auto attr = input.get_str_attr(constraint.attribute());
          if (!attr) {
            if (!constraint.optional() ||
                input.has_attr(constraint.attribute())) {
              return false;
            }
          } else {
            if (!constraint(*attr)) {
              return false;
            }
          }
        } else if (std::holds_alternative<int32_t>(constraint.operand())) {
          auto attr = input.get_int_attr(constraint.attribute());
          if (!attr) {
            if (!constraint.optional() ||
                input.has_attr(constraint.attribute())) {
              return false;
            }
          } else {
            if (!constraint(*attr)) {
              return false;
            }
          }
        } else if (std::holds_alternative<float>(constraint.operand())) {
          auto attr = input.get_float_attr(constraint.attribute());
          if (!attr) {
            if (!constraint.optional() ||
                input.has_attr(constraint.attribute())) {
              return false;
            }
          } else {
            if (!constraint(*attr)) {
              return false;
            }
          }
        } else if (std::holds_alternative<const std::vector<Constraint>*>(
                       constraint.operand())) {
          auto attr = input.get_nested_component(constraint.attribute());
          if (!attr) {
            if (!constraint.optional() ||
                input.has_attr(constraint.attribute())) {
              return false;
            }
          } else {
            if (!constraint(**attr)) {
              return false;
            }
          }
        } else {
          Support::Logger::error("CONSTRAINT",
                                 "Constraint operand has unkown type.");
          return false;
        }
      }
      return true;
    default:
      Support::Logger::warning("CONSTRAINT",
                               "Unsupported operation for map operand.");
      return false;
  }
}

template <>
inline bool Constraint::Container<std::vector<Constraint>>::operator==(
    const Constraint::Container<std::vector<Constraint>>& other) const {
  if (operation_name != other.operation_name) {
    return false;
  }

  if (operation_name != PubSub::ConstraintPredicates::Predicate::MAP_ALL) {
    Support::Logger::warning("CONSTRAINT",
                             "Unsupported operation for map operand.");
    return false;
  }

  for (const auto& constraint : operand) {
    if (std::find(other.operand.begin(), other.operand.end(), constraint) ==
        other.operand.end()) {
      return false;
    }
  }
  return true;
}

template <>
inline bool Constraint::Container<std::vector<Constraint>>::operator<(
    const Constraint::Container<std::vector<Constraint>>& other) const {
  // assert(operation == other.operation ==
  // PubSub::ConstraintPredicates::Predicate::MAP_ALL);

  std::vector left(operand);
  std::vector right(other.operand);

  std::sort(left.begin(), left.end());
  std::sort(right.begin(), right.end());

  auto right_item = right.begin();
  for (const auto& left_item : left) {
    if (left_item < *right_item) {
      return true;
    } else if (*right_item < left_item) {
      return false;
    }
    std::advance(right_item, 1);
  }
  printf("true\n");
  return true;
}

// Requires explicit definition due to the explicit template instantiations
// above
inline bool Constraint::operator<(const Constraint& other) const {
  return _attribute < other._attribute ||
         (_attribute == other._attribute && _optional < other._optional) ||
         (_attribute == other._attribute && _optional == other._optional &&
          _operand < other._operand);
}

}  //  namespace uActor::PubSub
