#pragma once

#include <functional>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

#include "constraint.hpp"
#include "subscription.hpp"

namespace uActor::PubSub {

struct SubscriptionCounter {
  size_t required = 0;
  size_t optional = 0;
};

template <template <typename> typename Allocator, typename AllocatorConfig>
using Counter = std::unordered_map<Subscription<Allocator, AllocatorConfig> *,
                                   SubscriptionCounter>;

template <template <typename> typename Allocator, typename AllocatorConfig>
struct ConstraintIndex {
  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  template <typename U>
  constexpr static auto make_allocator =
      AllocatorConfig::template make_allocator<U>;

  // optimized index for some cases

  using subscriptions_container =
      std::map<Subscription<Allocator, AllocatorConfig> *, bool,
               std::less<Subscription<Allocator, AllocatorConfig> *>,
               Allocator<std::pair<
                   Subscription<Allocator, AllocatorConfig> *const, bool>>>;
  std::map<AString, subscriptions_container, Support::StringCMP,
           Allocator<std::pair<const AString, subscriptions_container>>>
      string_equal{AllocatorConfig::template make_allocator<
          std::pair<const AString, subscriptions_container>>()};

  // Fallback to linar scan for all other types of constraints
  std::list<
      std::pair<Constraint,
                std::map<Subscription<Allocator, AllocatorConfig> *, bool>>,
      Allocator<std::pair<
          Constraint,
          std::map<Subscription<Allocator, AllocatorConfig> *, bool>>>>
      index{AllocatorConfig::template make_allocator<std::pair<
          Constraint,
          std::map<Subscription<Allocator, AllocatorConfig> *, bool>>>()};

  void insert(Constraint c, Subscription<Allocator, AllocatorConfig> *sub_ptr,
              bool opt) {
    auto operand = c.operand();
    if (std::holds_alternative<std::string>(operand) &&
        c.predicate() == ConstraintPredicates::Predicate::EQ) {
      auto string_op = std::get<std::string>(operand);
      if (const auto &[index_it, inserted] = string_equal.try_emplace(
              AString(string_op, make_allocator<AString>()),
              subscriptions_container{
                  {{sub_ptr, opt}},
                  make_allocator<
                      std::pair<Subscription<Allocator, AllocatorConfig> *const,
                                bool>>()});
          !inserted) {
        index_it->second.try_emplace(sub_ptr, opt);
      }
      return;
    }

    if (auto index_it =
            std::find_if(index.begin(), index.end(),
                         [&c](const auto &item) { return item.first == c; });
        index_it != index.end()) {
      index_it->second.try_emplace(sub_ptr, opt);
    } else {
      index.push_back(std::make_pair(
          std::move(c),
          std::map<Subscription<Allocator, AllocatorConfig> *, bool>{
              {sub_ptr, opt}}));
    }
  }

  bool remove(const Constraint &c,
              Subscription<Allocator, AllocatorConfig> *sub_ptr) {
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
      if (auto index_it =
              std::find_if(index.begin(), index.end(),
                           [&c](const auto &item) { return item.first == c; });
          index_it != index.end()) {
        if (index_it->second.size() == 1) {
          index.erase(index_it);
        } else {
          index_it->second.erase(sub_ptr);
        }
      }
    }
    return index.size() == 0 && string_equal.size() == 0;
  }

  void check(std::variant<AString, int32_t, float> value,
             Counter<Allocator, AllocatorConfig> *counter) {
    if (std::holds_alternative<AString>(value)) {
      if (auto index_it = string_equal.find(std::get<AString>(value));
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

    for (const auto &[constraint, subscriptions] : index) {
      if (std::holds_alternative<AString>(value) &&
          !constraint(std::get<AString>(value))) {
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

}  //  namespace uActor::PubSub
