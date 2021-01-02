
#include "pubsub/constraint.hpp"

#include <cstddef>
#include <utility>

namespace uActor::PubSub {

bool Constraint::operator()(std::string_view input) const {
  if (std::holds_alternative<Container<tracked_string>>(_operand)) {
    return (std::get<Container<tracked_string>>(_operand))
        .match(tracked_string(input));
  } else {
    return false;
  }
}

bool Constraint::operator()(int32_t input) const {
  if (std::holds_alternative<Container<int32_t>>(_operand)) {
    return (std::get<Container<int32_t>>(_operand)).match(input);
  } else {
    return false;
  }
}

bool Constraint::operator()(float input) const {
  if (std::holds_alternative<Container<float>>(_operand)) {
    return (std::get<Container<float>>(_operand)).match(input);
  } else {
    return false;
  }
}

bool Constraint::operator==(const Constraint& other) const {
  return other._attribute == _attribute && other._operand == _operand;
}

std::string Constraint::serialize() const {
  std::string serialized = std::string(_attribute);

  if (std::holds_alternative<Container<tracked_string>>(_operand)) {
    const Container<tracked_string>& cont =
        std::get<Container<tracked_string>>(_operand);
    serialized += ",s,";
    serialized +=
        std::string(ConstraintPredicates::name(cont.operation_name)) + ",";
    serialized += cont.operand;
  } else if (std::holds_alternative<Container<int32_t>>(_operand)) {
    const Container<int32_t>& cont = std::get<Container<int32_t>>(_operand);
    serialized += ",i,";
    serialized +=
        std::string(ConstraintPredicates::name(cont.operation_name)) + ",";
    serialized += std::to_string(cont.operand);
  } else if (std::holds_alternative<Container<float>>(_operand)) {
    const Container<float>& cont = std::get<Container<float>>(_operand);
    serialized += ",f,";
    serialized +=
        std::string(ConstraintPredicates::name(cont.operation_name)) + ",";
    serialized += std::to_string(cont.operand);
  }

  return std::move(serialized);
}

std::optional<Constraint> Constraint::deserialize(std::string_view serialized,
                                                  bool optional) {
  size_t start_index = 0;
  size_t index = serialized.find_first_of(',', start_index);
  std::string attribute_name =
      std::string(serialized.substr(start_index, index - start_index));

  start_index = index + 1;
  index = serialized.find_first_of(',', start_index);
  std::string_view type_string =
      serialized.substr(start_index, index - start_index);

  start_index = index + 1;
  index = serialized.find_first_of(',', start_index);
  std::string_view operation_name =
      serialized.substr(start_index, index - start_index);
  auto predicate = ConstraintPredicates::from_string(operation_name);
  if (!predicate) {
    return std::nullopt;
  }

  start_index = index + 1;
  index = serialized.find_first_of(',', index + 1);
  std::string_view operator_string =
      serialized.substr(start_index, index - start_index);

  if (type_string == "f") {
    return Constraint(attribute_name, std::stof(std::string(operator_string)),
                      *predicate, optional);
  } else if (type_string == "i") {
    return Constraint(attribute_name, std::stoi(std::string(operator_string)),
                      *predicate, optional);
  } else {
    return Constraint(attribute_name, std::string(operator_string), *predicate,
                      optional);
  }
}

std::variant<std::monostate, std::string, int32_t, float> Constraint::operand()
    const {
  if (std::holds_alternative<Container<tracked_string>>(_operand)) {
    const auto& ct = std::get<Container<tracked_string>>(_operand);
    return std::string(ct.operand);
  } else if (std::holds_alternative<Container<int32_t>>(_operand)) {
    const auto& ct = std::get<Container<std::int32_t>>(_operand);
    return ct.operand;
  } else if (std::holds_alternative<Container<float>>(_operand)) {
    const auto& ct = std::get<Container<float>>(_operand);
    return ct.operand;
  }
  return std::variant<std::monostate, std::string, int32_t, float>();
}

ConstraintPredicates::Predicate Constraint::predicate() const {
  if (std::holds_alternative<Container<tracked_string>>(_operand)) {
    const auto& ct = std::get<Container<tracked_string>>(_operand);
    return ct.operation_name;
  } else if (std::holds_alternative<Container<int32_t>>(_operand)) {
    const auto& ct = std::get<Container<int32_t>>(_operand);
    return ct.operation_name;
  } else if (std::holds_alternative<Container<float>>(_operand)) {
    const auto& ct = std::get<Container<float>>(_operand);
    return ct.operation_name;
  }
  printf("warning: requested predicate of undefined constraint\n");
  return ConstraintPredicates::Predicate::EQ;
}

const char* ConstraintPredicates::name(uint32_t tag) {
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
    default:
      return nullptr;
  }
}

std::optional<ConstraintPredicates::Predicate>
ConstraintPredicates::from_string(std::string_view name) {
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
}  // namespace uActor::PubSub
