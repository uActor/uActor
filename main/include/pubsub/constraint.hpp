#ifndef MAIN_INCLUDE_PUBSUB_CONSTRAINT_HPP_
#define MAIN_INCLUDE_PUBSUB_CONSTRAINT_HPP_

#include <cstdint>
#include <optional>
#include <functional>
#include <variant>
#include <string>
#include <string_view>
#include <utility>

namespace PubSub {

struct ConstraintPredicates {
  constexpr static const uint32_t MAX_INDEX = 6;
  enum Predicate : uint32_t { EQ = 1, NE, LT, GT, GE, LE };
  constexpr static const char* name(uint32_t tag) {
    switch (tag) {
      case 1:
        return "EQ";
      case 2:
        return "NE";
      case 3:
        return "LT";
      case 4:
        return "GT";
      case 5:
        return "GE";
      case 6:
        return "LE";
    }
    return nullptr;
  }
  static std::optional<Predicate> from_string(std::string_view name) {
    if (name == "EQ") {
      return Predicate::EQ;
    }
    if (name == "NE") {
      return Predicate::NE;
    }
    if (name == "LT") {
      return Predicate::LT;
    }
    if (name == "GT") {
      return Predicate::GT;
    }
    if (name == "GE") {
      return Predicate::GE;
    }
    if (name == "LE") {
      return Predicate::LE;
    } else {
      printf("Deserialization error %s\n", name.data());
      return std::nullopt;
    }
  }
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

  bool operator()(std::string_view input) const {
    if (std::holds_alternative<Container<std::string>>(_operand)) {
      return (std::get<Container<std::string>>(_operand))
          .operation(std::string(input),
                     std::get<Container<std::string>>(_operand).operand);
    } else {
      return false;
    }
  }

  bool operator()(int32_t input) const {
    if (std::holds_alternative<Container<int32_t>>(_operand)) {
      return (std::get<Container<int32_t>>(_operand))
          .operation(input, std::get<Container<int32_t>>(_operand).operand);
    } else {
      return false;
    }
  }

  bool operator()(float input) const {
    if (std::holds_alternative<Container<float>>(_operand)) {
      return (std::get<Container<float>>(_operand))
          .operation(input, std::get<Container<float>>(_operand).operand);
    } else {
      return false;
    }
  }

  bool operator==(const Constraint& other) const {
    return other._attribute == _attribute && other._operand == _operand;
  }

  bool optional() const { return _optional; }

  const std::string_view attribute() const {
    return std::string_view(_attribute);
  }

  std::string serialize() {
    std::string serialized = _attribute;

    if (std::holds_alternative<Container<std::string>>(_operand)) {
      Container<std::string>& cont = std::get<Container<std::string>>(_operand);
      serialized += ",s,";
      serialized +=
          std::string(ConstraintPredicates::name(cont.operation_name)) + ",";
      serialized += cont.operand;
    } else if (std::holds_alternative<Container<int32_t>>(_operand)) {
      Container<int32_t>& cont = std::get<Container<int32_t>>(_operand);
      serialized += ",i,";
      serialized +=
          std::string(ConstraintPredicates::name(cont.operation_name)) + ",";
      serialized += std::to_string(cont.operand);
    } else if (std::holds_alternative<Container<float>>(_operand)) {
      Container<float>& cont = std::get<Container<float>>(_operand);
      serialized += ",f,";
      serialized +=
          std::string(ConstraintPredicates::name(cont.operation_name)) + ",";
      serialized += std::to_string(cont.operand);
    }

    return std::move(serialized);
  }

  static std::optional<Constraint> deserialize(std::string_view serialized,
                                               bool optional) {
    size_t start_index = 0;
    size_t index = serialized.find_first_of(",", start_index);
    std::string attribute_name =
        std::string(serialized.substr(start_index, index - start_index));

    start_index = index + 1;
    index = serialized.find_first_of(",", start_index);
    std::string_view type_string =
        serialized.substr(start_index, index - start_index);

    start_index = index + 1;
    index = serialized.find_first_of(",", start_index);
    std::string_view operation_name =
        serialized.substr(start_index, index - start_index);
    auto predicate = ConstraintPredicates::from_string(operation_name);
    if (!predicate) {
      return std::nullopt;
    }

    start_index = index + 1;
    index = serialized.find_first_of(",", index + 1);
    std::string_view operator_string =
        serialized.substr(start_index, index - start_index);

    if (type_string == "f") {
      return Constraint(attribute_name, std::stof(std::string(operator_string)),
                        *predicate, optional);
    } else if (type_string == "i") {
      return Constraint(attribute_name, std::stoi(std::string(operator_string)),
                        *predicate, optional);
    } else {
      return Constraint(attribute_name, std::string(operator_string),
                        *predicate, optional);
    }
  }

 private:
  std::string _attribute;
  std::variant<std::monostate, Container<std::string>, Container<int32_t>,
               Container<float>>
      _operand;
  bool _optional;
};
}  //  namespace PubSub

#endif  //  MAIN_INCLUDE_PUBSUB_CONSTRAINT_HPP_
