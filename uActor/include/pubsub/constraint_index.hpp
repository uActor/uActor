#pragma once

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "allocator_configuration.hpp"
#include "constraint.hpp"
#include "subscription.hpp"

namespace uActor::PubSub {

struct SubscriptionCounter {
  size_t required = 0;
  size_t optional = 0;
};

template <template <typename> typename Allocator>
using Counter =
    std::unordered_map<Subscription<Allocator>*, SubscriptionCounter>;

template <template <typename> typename Allocator>
struct ConstraintIndex {
  using allocator_type = Allocator<ConstraintIndex>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  using subscriptions_container =
      std::vector<std::pair<Subscription<Allocator>*, bool>,
                  Allocator<std::pair<Subscription<Allocator>*, bool>>>;

  using ConstraintItem =
      std::pair<std::shared_ptr<const Constraint>, subscriptions_container>;
  // using ConstraintItemPointer = std::shared_ptr<ConstraintItem>;

  // optimized index for some cases
  std::vector<ConstraintItem, Allocator<ConstraintItem>> string_equal;

  // Fallback to linar scan for all other types of constraints
  std::vector<ConstraintItem, Allocator<ConstraintItem>> index;

  template <typename PAllocator = Allocator<ConstraintIndex>>
  explicit ConstraintIndex(std::allocator_arg_t, const PAllocator& allocator)
      : string_equal(allocator), index(allocator) {}

  template <typename PAllocator = Allocator<ConstraintIndex>>
  ConstraintIndex(std::allocator_arg_t, const PAllocator& allocator,
                  ConstraintIndex&& other)
      : string_equal(std::move(other.string_equal), allocator),
        index(std::move(other.index), allocator) {}

  template <typename PAllocator = Allocator<ConstraintIndex>>
  ConstraintIndex(std::allocator_arg_t, const PAllocator& allocator,
                  const ConstraintIndex& other)
      : string_equal(other.string_equal, allocator),
        index(other.index, allocator) {}

  std::shared_ptr<const Constraint> insert(Constraint c,
                                           Subscription<Allocator>* sub_ptr,
                                           bool opt) {
    if (std::holds_alternative<std::string_view>(c.operand()) &&
        c.predicate() ==
            ConstraintPredicates::Predicate::EQ) {  // Use sorted string index
      auto string_operand = std::get<std::string_view>(c.operand());
      auto index_it = std::lower_bound(
          string_equal.begin(), string_equal.end(), string_operand,
          [](const auto& item, const auto& value) {
            return std::get<std::string_view>(item.first->operand()) < value;
          });
      if (index_it == string_equal.end() ||
          string_operand !=
              std::get<std::string_view>(index_it->first->operand())) {
        auto it = string_equal.emplace(
            index_it, std::make_shared<Constraint>(std::move(c)),
            std::initializer_list<typename subscriptions_container::value_type>(
                {{sub_ptr, opt}}));
        return it->first;
      } else if (index_it != string_equal.end()) {
        auto& [stored_constraint, relevant_subscriptions] = *index_it;
        auto existing_stored_sub_it = std::find_if(
            relevant_subscriptions.begin(), relevant_subscriptions.end(),
            [sub_ptr](const auto& item) { return item.first == sub_ptr; });
        if (existing_stored_sub_it == relevant_subscriptions.end()) {
          index_it->second.push_back(std::make_pair(sub_ptr, opt));
        }
        return stored_constraint;
      }
    } else {  // Use fallback index
      auto index_it =
          std::find_if(index.begin(), index.end(),
                       [&c](const auto& item) { return *item.first == c; });
      if (index_it == index.end()) {
        index.emplace_back(
            std::make_shared<Constraint>(std::move(c)),
            std::initializer_list<typename subscriptions_container::value_type>(
                {{sub_ptr, opt}}));
        return index.back().first;
      } else {
        auto& [stored_constraint_ptr, relevant_subscriptions] = *index_it;
        auto existing_stored_sub_it = std::find_if(
            relevant_subscriptions.begin(), relevant_subscriptions.end(),
            [sub_ptr](const auto& item) { return item.first == sub_ptr; });
        if (existing_stored_sub_it == relevant_subscriptions.end()) {
          index_it->second.push_back(std::make_pair(sub_ptr, opt));
        }
        return stored_constraint_ptr;
      }
    }
    return nullptr;
  }

  bool remove(const Constraint& c, Subscription<Allocator>* sub_ptr) {
    auto operand = c.operand();

    if (std::holds_alternative<std::string_view>(operand) &&
        c.predicate() == ConstraintPredicates::Predicate::EQ) {
      auto string_operand = std::get<std::string_view>(operand);
      auto index_it = std::lower_bound(
          string_equal.begin(), string_equal.end(), string_operand,
          [](const auto& item, const auto& value) {
            return std::get<std::string_view>(item.first->operand()) < value;
          });
      if (index_it != string_equal.end() &&
          std::get<std::string_view>(index_it->first->operand()) ==
              string_operand) {
        auto& relevant_constraints = index_it->second;
        if (relevant_constraints.size() == 1) {
          string_equal.erase(index_it);
        } else {
          auto erase_it = std::find_if(
              relevant_constraints.begin(), relevant_constraints.end(),
              [sub_ptr](const auto& item) { return item.first == sub_ptr; });
          if (erase_it != relevant_constraints.end()) {
            relevant_constraints.erase(erase_it);
          }
        }
      }
    } else {
      auto index_it =
          std::find_if(index.begin(), index.end(),
                       [&c](const auto& item) { return *item.first == c; });
      if (index_it != index.end()) {
        auto& [_stored_constraint, relevant_subscriptions] = *index_it;
        if (relevant_subscriptions.size() == 1) {
          index.erase(index_it);
        } else {
          auto erase_it = std::find_if(
              relevant_subscriptions.begin(), relevant_subscriptions.end(),
              [sub_ptr](const auto& item) { return item.first == sub_ptr; });
          if (erase_it != relevant_subscriptions.end()) {
            relevant_subscriptions.erase(erase_it);
          }
        }
      }
    }
    return index.size() == 0 && string_equal.size() == 0;
  }

  void check(std::variant<std::string_view, int32_t, float> value,
             Counter<Allocator>* counter) {
    if (std::holds_alternative<std::string_view>(value)) {
      auto index_it = std::lower_bound(
          string_equal.begin(), string_equal.end(),
          std::get<std::string_view>(value),
          [](const auto& item, const auto& value) {
            return std::get<std::string_view>(item.first->operand()) < value;
          });
      if (index_it != string_equal.end() &&
          std::get<std::string_view>(index_it->first->operand()) ==
              std::get<std::string_view>(value)) {
        auto& [_stored_constraint, relevant_subscriptions] = *index_it;
        for (auto& [sub_ptr, optional] : relevant_subscriptions) {
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

    for (const auto& item_ptr : index) {
      auto& [stored_constraint_ptr, relevant_subscriptions] = item_ptr;
      if (std::holds_alternative<std::string_view>(value) &&
          !(*stored_constraint_ptr)(std::get<std::string_view>(value))) {
        continue;
      }
      if (std::holds_alternative<int32_t>(value) &&
          !(*stored_constraint_ptr)(std::get<int32_t>(value))) {
        continue;
      }
      if (std::holds_alternative<float>(value) &&
          !(*stored_constraint_ptr)(std::get<float>(value))) {
        continue;
      }

      for (auto [sub_ptr, optional] : relevant_subscriptions) {
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

}  //  namespace uActor::PubSub
