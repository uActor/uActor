#ifndef UACTOR_INCLUDE_PUBSUB_CONSTRAINT_HPP_
#define UACTOR_INCLUDE_PUBSUB_CONSTRAINT_HPP_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace uActor::PubSub {

struct ConstraintPredicates {
  constexpr static const uint32_t MAX_INDEX = 6;
  enum Predicate : uint32_t { EQ = 1, NE, LT, GT, GE, LE };

  static const char* name(uint32_t tag);
  static std::optional<Predicate> from_string(std::string_view name);
};

class Constraint {
  template <typename T>
  struct Container {
    Container(T operand, std::function<bool(const T&, const T&)> operation,
              ConstraintPredicates::Predicate operation_name)
        : operand(operand),
          operation(operation),
          operation_name(operation_name) {}
    T operand;
    std::function<bool(const T&, const T&)> operation;
    ConstraintPredicates::Predicate operation_name;

    bool operator==(const Container<T>& other) const {
      return operand == other.operand;
    }
  };

 public:
  // TODO(raphaelhetzel) combine once we use C++20
  Constraint(
      std::string attribute, std::string oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false) {
    setup(attribute, oper, op, optional);
  }

  Constraint(
      std::string attribute, int32_t oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false) {
    setup(attribute, oper, op, optional);
  }

  Constraint(
      std::string attribute, float oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ,
      bool optional = false) {
    setup(attribute, oper, op, optional);
  }

  template <typename T>
  void setup(std::string attribute, T operand,
             ConstraintPredicates::Predicate predicate, bool optional) {
    switch (predicate) {
      case ConstraintPredicates::Predicate::EQ:
        _operand = Container<T>(operand, std::equal_to<T>(), predicate);
        break;
      case ConstraintPredicates::Predicate::LT:
        _operand = Container<T>(operand, std::less<T>(), predicate);
        break;
      case ConstraintPredicates::Predicate::GT:
        _operand = Container<T>(operand, std::greater<T>(), predicate);
        break;
      case ConstraintPredicates::Predicate::GE:
        _operand = Container<T>(operand, std::greater_equal<T>(), predicate);
        break;
      case ConstraintPredicates::Predicate::LE:
        _operand = Container<T>(operand, std::less_equal<T>(), predicate);
        break;
      case ConstraintPredicates::Predicate::NE:
        _operand = Container<T>(operand, std::not_equal_to<T>(), predicate);
        break;
    }
    if (attribute.at(0) == '?') {
      _attribute = attribute.substr(1);
      _optional = true;
    } else {
      _attribute = attribute;
      _optional = optional;
    }
  }

  bool operator()(std::string_view input) const;

  bool operator()(int32_t input) const;

  bool operator()(float input) const;

  bool operator==(const Constraint& other) const;

  bool optional() const { return _optional; }

  const std::string_view attribute() const {
    return std::string_view(_attribute);
  }

  std::string serialize() const;

  static std::optional<Constraint> deserialize(std::string_view serialized,
                                               bool optional);

  std::variant<std::monostate, std::string, int32_t, float> operand() const;

  ConstraintPredicates::Predicate predicate() const;

 private:
  std::string _attribute;
  std::variant<std::monostate, Container<std::string>, Container<int32_t>,
               Container<float>>
      _operand;
  bool _optional;
};
}  //  namespace uActor::PubSub

#endif  //  UACTOR_INCLUDE_PUBSUB_CONSTRAINT_HPP_
