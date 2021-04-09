#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "allocator_configuration.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

struct ConstraintPredicates {
  constexpr static const uint32_t MAX_INDEX = 6;
  enum Predicate : uint32_t { EQ = 1, NE, LT, GT, GE, LE };

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
        : operand(operand), operation_name(operation_name) {}

    T operand;
    ConstraintPredicates::Predicate operation_name;

    [[nodiscard]] bool match(T input) const {
      switch (operation_name) {
        case ConstraintPredicates::Predicate::EQ:
          return std::equal_to<T>()(input, operand);
        case ConstraintPredicates::Predicate::LT:
          return std::less<T>()(input, operand);
        case ConstraintPredicates::Predicate::GT:
          return std::greater<T>()(input, operand);
        case ConstraintPredicates::Predicate::GE:
          return std::greater_equal<T>()(input, operand);
        case ConstraintPredicates::Predicate::LE:
          return std::less_equal<T>()(input, operand);
        case ConstraintPredicates::Predicate::NE:
          return std::not_equal_to<T>()(input, operand);
      }
      return false;
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

  bool operator==(const Constraint& other) const;

  bool operator<(const Constraint& other) const {
    return _attribute < other._attribute ||
           (_attribute == other._attribute && _optional < other._optional) ||
           (_attribute == other._attribute && _optional == other._optional &&
            _operand < other._operand);
  }

  [[nodiscard]] bool optional() const { return _optional; }

  [[nodiscard]] const std::string_view attribute() const {
    return std::string_view(_attribute);
  }

  [[nodiscard]] std::string serialize() const;

  static std::optional<Constraint> deserialize(std::string_view serialized,
                                               bool optional);

  [[nodiscard]] std::variant<std::monostate, std::string_view, int32_t, float>
  operand() const;

  [[nodiscard]] ConstraintPredicates::Predicate predicate() const;

 private:
  AString _attribute;
  std::variant<std::monostate, Container<AString>, Container<int32_t>,
               Container<float>>
      _operand;
  bool _optional;
};
}  //  namespace uActor::PubSub
