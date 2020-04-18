#include "pubsub/filter.hpp"

#include <utility>
#include <variant>

#include "support/string_helper.hpp"

namespace uActor::PubSub {

Filter::Filter(std::initializer_list<Constraint> constraints) {
  for (Constraint c : constraints) {
    if (c.optional()) {
      optional.push_back(std::move(c));
    } else {
      required.push_back(std::move(c));
    }
  }
}

Filter::Filter(std::list<Constraint>&& constraints) {
  for (Constraint c : constraints) {
    if (c.optional()) {
      optional.push_back(std::move(c));
    } else {
      required.push_back(std::move(c));
    }
  }
}

void Filter::clear() {
  required.clear();
  optional.clear();
}

bool Filter::matches(const Publication& publication) const {
  if (!check_required(publication)) {
    return false;
  }
  return check_optionals(publication);
}

bool Filter::check_required(const Publication& publication) const {
  for (const Constraint& constraint : required) {
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

bool Filter::check_optionals(const Publication& publication) const {
  for (const Constraint& constraint : optional) {
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
    }
  }
  return true;
}

bool Filter::operator==(const Filter& other) const {
  if (required.size() != other.required.size() ||
      optional.size() != other.optional.size()) {
    return false;
  }
  for (const Constraint& c : required) {
    if (std::find(other.required.begin(), other.required.end(), c) ==
        other.required.end()) {
      return false;
    }
  }
  for (const Constraint& c : optional) {
    if (std::find(other.optional.begin(), other.optional.end(), c) ==
        other.optional.end()) {
      return false;
    }
  }
  return true;
}

std::string Filter::serialize() {
  std::string serialized;
  for (auto& constraint : required) {
    serialized += constraint.serialize() + "^";
  }
  if (serialized.length() >= 2) {
    serialized.replace(serialized.length() - 1, 1, "?");
  }

  for (auto& constraint : optional) {
    serialized += constraint.serialize() + "^";
  }

  if (serialized.length() >= 2) {
    serialized.resize(serialized.length() - 1);
  }
  return std::move(serialized);
}

std::optional<Filter> Filter::deserialize(std::string_view serialized) {
  Filter filter;

  if (serialized.length() == 0) {
    return std::move(filter);
  }

  size_t optional_index = serialized.find("?");

  std::string_view required_args = serialized.substr(0, optional_index);

  for (std::string_view constraint_string :
       Support::StringHelper::string_split(required_args, "^")) {
    auto constraint = Constraint::deserialize(constraint_string, false);
    if (constraint) {
      filter.required.emplace_back(std::move(*constraint));
    } else {
      return std::nullopt;
    }
  }

  if (optional_index != std::string_view::npos) {
    std::string_view optional_args = serialized.substr(optional_index);
    for (std::string_view constraint_string :
         Support::StringHelper::string_split(optional_args, "^")) {
      auto constraint = Constraint::deserialize(constraint_string, true);
      if (constraint) {
        filter.optional.emplace_back(std::move(*constraint));
      } else {
        return std::nullopt;
      }
    }
  }

  return std::move(filter);
}

}  // namespace uActor::PubSub
