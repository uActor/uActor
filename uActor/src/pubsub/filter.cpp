#include "pubsub/filter.hpp"

#include <utility>
#include <variant>

#include "support/string_helper.hpp"

namespace uActor::PubSub {

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
      } else if (std::holds_alternative<std::shared_ptr<Publication::Map>>(
                     attr) &&
                 !constraint(
                     *std::get<std::shared_ptr<Publication::Map>>(attr))) {
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
      } else if (std::holds_alternative<std::shared_ptr<Publication::Map>>(
                     attr) &&
                 !constraint(
                     *std::get<std::shared_ptr<Publication::Map>>(attr))) {
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

void Filter::add_constraint_to_map(const Constraint& constraint,
                                   Publication::Map* map) const {
  if (std::holds_alternative<std::monostate>(constraint.operand())) {
    return;
  }

  auto current_attribute = std::make_shared<Publication::Map>();
  current_attribute->set_attr(
      "operator", ConstraintPredicates::name(constraint.predicate()));

  if (std::holds_alternative<std::string_view>(constraint.operand())) {
    current_attribute->set_attr(
        "operand", std::get<std::string_view>(constraint.operand()));
  } else if (std::holds_alternative<int32_t>(constraint.operand())) {
    current_attribute->set_attr("operand",
                                std::get<int32_t>(constraint.operand()));
  } else if (std::holds_alternative<float>(constraint.operand())) {
    current_attribute->set_attr("operand",
                                std::get<float>(constraint.operand()));
  } else if (std::holds_alternative<const std::vector<Constraint>*>(
                 constraint.operand())) {
    auto elem = std::make_shared<Publication::Map>();
    for (const auto& elem_constraint :
         *std::get<const std::vector<Constraint>*>(constraint.operand())) {
      add_constraint_to_map(elem_constraint, elem.get());
    }
    current_attribute->set_attr("operand", std::move(elem));
  }

  current_attribute->set_attr("optional", constraint.optional() ? 1 : 0);

  if (map->has_attr(constraint.attribute())) {
    auto handle = map->get_nested_component(constraint.attribute());
    if ((*handle)->has_attr("_m")) {
      (*handle)->set_attr(std::to_string((*handle)->size() - 1),
                          std::move(current_attribute));
    } else {
      auto container = std::make_shared<Publication::Map>();
      container->set_attr("_m", 1);
      container->set_attr("0", std::move(*handle));
      container->set_attr("1", std::move(current_attribute));
      map->set_attr(constraint.attribute(), std::move(container));
    }
  } else {
    map->set_attr(constraint.attribute(), std::move(current_attribute));
  }
}

std::shared_ptr<PubSub::Publication::Map> Filter::to_publication_map() const {
  auto map = std::make_shared<PubSub::Publication::Map>();

  for (const auto& item : required) {
    add_constraint_to_map(item, map.get());
  }

  for (const auto& item : optional) {
    add_constraint_to_map(item, map.get());
  }

  return map;
}

std::optional<Constraint> Filter::parse_constraint_from_map(
    std::string_view key, const Publication::Map& item) {
  auto raw_pred = item.get_str_attr("operator");
  auto raw_optional = item.get_int_attr("optional");
  auto operand_var = item.get_attr("operand");

  auto pred = ConstraintPredicates::EQ;
  if (raw_pred) {
    auto parsed_pred = ConstraintPredicates::from_string(*raw_pred);
    if (parsed_pred) {
      pred = *parsed_pred;
    } else {
      return std::nullopt;
    }
  }
  auto optional = raw_optional ? (*raw_optional == 1) : false;

  if (std::holds_alternative<std::monostate>(operand_var)) {
    return std::nullopt;
  }

  if (std::holds_alternative<std::string_view>(operand_var)) {
    return Constraint(std::string(key), std::get<std::string_view>(operand_var),
                      pred, optional);
  } else if (std::holds_alternative<int32_t>(operand_var)) {
    return Constraint(std::string(key), std::get<int32_t>(operand_var), pred,
                      optional);
  } else if (std::holds_alternative<float>(operand_var)) {
    return Constraint(std::string(key), std::get<float>(operand_var), pred,
                      optional);
  } else if (std::holds_alternative<std::shared_ptr<Publication::Map>>(
                 operand_var)) {
    std::vector<Constraint> constraints;
    for (const auto& [attribute, data] :
         *std::get<std::shared_ptr<Publication::Map>>(operand_var)) {
      if (std::holds_alternative<std::shared_ptr<Publication::Map>>(data)) {
        auto item = std::get<std::shared_ptr<Publication::Map>>(data);
        if (item->has_attr("_m")) {
          for (size_t i = 0; i < item->size() - 1; i++) {
            auto map = item->get_nested_component(std::to_string(i));
            if (map) {
              auto constr = parse_constraint_from_map(attribute, **map);
              if (constr) {
                constraints.emplace_back(std::move(*constr));
              } else {
                return std::nullopt;
              }
            } else {
              return std::nullopt;
            }
          }
        } else {
          auto constr = parse_constraint_from_map(attribute, *item);
          if (constr) {
            constraints.emplace_back(std::move(*constr));
          } else {
            return std::nullopt;
          }
        }
      } else {
        return std::nullopt;
      }
    }
    return Constraint(std::string(key), std::move(constraints), pred, optional);
  }
  return std::nullopt;
}

std::optional<Filter> Filter::from_publication_map(
    const Publication::Map& map) {
  Filter filter;
  for (const auto& [attribute, data] : map) {
    if (std::holds_alternative<std::shared_ptr<Publication::Map>>(data)) {
      auto item = std::get<std::shared_ptr<Publication::Map>>(data);
      if (item->has_attr("_m")) {
        for (size_t i = 0; i < item->size() - 1; i++) {
          auto map = item->get_nested_component(std::to_string(i));
          if (map) {
            auto c = parse_constraint_from_map(attribute, **map);
            if (c->optional()) {
              if (c) {
                filter.optional.emplace_back(std::move(*c));
              } else {
                return std::nullopt;
              }
            } else {
              if (c) {
                filter.required.emplace_back(std::move(*c));
              } else {
                return std::nullopt;
              }
            }
          }
        }
      } else {
        auto c = parse_constraint_from_map(attribute, *item);
        if (c->optional()) {
          if (c) {
            filter.optional.emplace_back(std::move(*c));
          } else {
            return std::nullopt;
          }
        } else {
          if (c) {
            filter.required.emplace_back(std::move(*c));
          } else {
            return std::nullopt;
          }
        }
      }
    }
  }

  return filter;
}

}  // namespace uActor::PubSub
