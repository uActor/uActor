#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "support/memory_manager.hpp"

namespace uActor::PubSub {

struct ConstraintPredicates {
  constexpr static const uint32_t MAX_INDEX = 6;
  enum Predicate : uint32_t { EQ = 1, NE, LT, GT, GE, LE };

  static const char* name(uint32_t tag);
  static std::optional<Predicate> from_string(std::string_view name);
};

class Constraint {
  using tracked_string =
      std::basic_string<char, std::char_traits<char>,
                        uActor::Support::TrackingAllocator<char>>;
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
      return operand == other.operand;
    }
  };

 public:
  // TODO(raphaelhetzel) combine once we use C++20
  Constraint(
      std::string attribute, std::string oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false)
      : _attribute(tracked_string(attribute,
                                  tracked_string::allocator_type(
                                      Support::TrackedRegions::ROUTING_STATE))),
        _operand(Container<tracked_string>(
            tracked_string(oper, tracked_string::allocator_type(
                                     Support::TrackedRegions::ROUTING_STATE)),
            op)),
        _optional(optional) {
    //    setup(AString(attribute), AString(oper), op, optional);
  }

  Constraint(
      std::string attribute, int32_t oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false)
      : _attribute(tracked_string(attribute,
                                  tracked_string::allocator_type(
                                      Support::TrackedRegions::ROUTING_STATE))),
        _operand(Container<int32_t>(oper, op)),
        _optional(optional) {
    //    setup(AString(attribute), oper, op, optional);
  }

  Constraint(
      std::string attribute, float oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false)
      : _attribute(tracked_string(attribute,
                                  tracked_string::allocator_type(
                                      Support::TrackedRegions::ROUTING_STATE))),
        _operand(Container<float>(oper, op)),
        _optional(optional) {
    //    setup(AString(attribute), oper, op, optional);
  }

  //  template <typename T>
  //  void setup(AString attribute, T operand,
  //             ConstraintPredicates::Predicate predicate, bool optional) {
  //    _operand = ;
  ////    if (attribute.at(0) == '?') {
  ////      _attribute =  attribute.substr(1);
  ////      _optional = true;
  ////    }
  ////    else {
  ////      _attribute = attribute;
  //      _optional = optional;
  ////    }
  //  }

  bool operator()(std::string_view input) const;

  bool operator()(int32_t input) const;

  bool operator()(float input) const;

  bool operator==(const Constraint& other) const;

  [[nodiscard]] bool optional() const { return _optional; }

  [[nodiscard]] const std::string_view attribute() const {
    return std::string_view(_attribute);
  }

  [[nodiscard]] std::string serialize() const;

  static std::optional<Constraint> deserialize(std::string_view serialized,
                                               bool optional);

  [[nodiscard]] std::variant<std::monostate, std::string, int32_t, float>
  operand() const;

  [[nodiscard]] ConstraintPredicates::Predicate predicate() const;

 private:
  tracked_string _attribute;
  std::variant<std::monostate, Container<tracked_string>, Container<int32_t>,
               Container<float>>
      _operand;
  bool _optional;
};
}  //  namespace uActor::PubSub
