#ifndef MAIN_INCLUDE_PUBSUB_CONSTRAINT_INDEX_HPP_
#define MAIN_INCLUDE_PUBSUB_CONSTRAINT_INDEX_HPP_

#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

#include "constraint.hpp"

namespace PubSub {

class Subscription;

struct SubscriptionCounter {
  size_t required = 0;
  size_t optional = 0;
};

using Counter = std::unordered_map<Subscription*, SubscriptionCounter>;

struct ConstraintIndex {
  // optimized index for some cases
  std::map<std::string, std::map<Subscription*, bool>> string_equal;

  // Fallback to linar scan for all other types of constraints
  std::list<std::pair<Constraint, std::map<Subscription*, bool>>> data;

  void insert(Constraint c, Subscription* sub_ptr, bool opt) {
    auto operand = c.operand();
    if (std::holds_alternative<std::string>(operand) &&
        c.predicate() == ConstraintPredicates::Predicate::EQ) {
      auto string_op = std::get<std::string>(operand);
      if (const auto& [index_it, inserted] = string_equal.try_emplace(
              std::move(string_op),
              std::map<Subscription*, bool>{{sub_ptr, opt}});
          !inserted) {
        index_it->second.try_emplace(sub_ptr, opt);
      }
      return;
    }

    if (auto data_it =
            std::find_if(data.begin(), data.end(),
                         [&c](const auto& item) { return item.first == c; });
        data_it != data.end()) {
      data_it->second.try_emplace(sub_ptr, opt);
    } else {
      data.push_back(std::make_pair(
          std::move(c), std::map<Subscription*, bool>{{sub_ptr, opt}}));
    }
  }

  bool remove(const Constraint& c, Subscription* sub_ptr) {
    auto operand = c.operand();
    if (std::holds_alternative<std::string>(operand) &&
        c.predicate() == ConstraintPredicates::Predicate::EQ) {
      auto string_op = std::get<std::string>(operand);
      if (auto index_it = string_equal.find(string_op);
          index_it != string_equal.end()) {
        if (index_it->second.size() == 1) {
          string_equal.erase(index_it);
        } else {
          index_it->second.erase(sub_ptr);
        }
      }
    } else {
      if (auto data_it =
              std::find_if(data.begin(), data.end(),
                           [&c](const auto& item) { return item.first == c; });
          data_it != data.end()) {
        if (data_it->second.size() == 1) {
          data.erase(data_it);
        } else {
          data_it->second.erase(sub_ptr);
        }
      }
    }
    return data.size() == 0 && string_equal.size() == 0;
  }

  void check(std::variant<std::string, int32_t, float> value,
             Counter* counter) {
    if (std::holds_alternative<std::string>(value)) {
      if (auto index_it = string_equal.find(std::get<std::string>(value));
          index_it != string_equal.end()) {
        for (auto [sub_ptr, optional] : index_it->second) {
          auto [counter_it, _inserted] =
              counter->try_emplace(sub_ptr, SubscriptionCounter());
          if (optional) {
            counter_it->second.optional++;
          } else {
            counter_it->second.required++;
          }
        }
      }
    }

    for (const auto& [constraint, subscriptions] : data) {
      if (std::holds_alternative<std::string>(value) &&
          !constraint(std::get<std::string>(value))) {
        continue;
      }
      if (std::holds_alternative<int32_t>(value) &&
          !constraint(std::get<int32_t>(value))) {
        continue;
      }
      if (std::holds_alternative<float>(value) &&
          !constraint(std::get<float>(value))) {
        continue;
      }

      for (auto [sub_ptr, optional] : subscriptions) {
        auto [counter_it, _inserted] =
            counter->try_emplace(sub_ptr, SubscriptionCounter());
        if (optional) {
          counter_it->second.optional++;
        } else {
          counter_it->second.required++;
        }
      }
    }
  }
};

#endif  //   MAIN_INCLUDE_PUBSUB_CONSTRAINT_INDEX_HPP_
}  //  namespace PubSub
