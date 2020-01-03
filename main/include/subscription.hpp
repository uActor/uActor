#ifndef MAIN_INCLUDE_SUBSCRIPTION_HPP_
#define MAIN_INCLUDE_SUBSCRIPTION_HPP_

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "publication.hpp"

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
};

class Constraint {
  template <typename T>
  struct Container {
    Container(T operand, std::function<bool(const T&, const T&)> operation)
        : operand(operand), operation(operation) {}
    T operand;
    std::function<bool(const T&, const T&)> operation;

    bool operator==(const Container<T>& other) const {
      return operand == other.operand;
    }
  };

 public:
  // TODO(raphaelhetzel) combine once we use C++20
  Constraint(
      std::string attribute, std::string oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ) {
    switch (op) {
      case ConstraintPredicates::Predicate::EQ:
        operand = Container<std::string>(oper, std::equal_to<std::string>());
        break;
      case ConstraintPredicates::Predicate::LT:
        operand = Container<std::string>(oper, std::less<std::string>());
        break;
      case ConstraintPredicates::Predicate::GT:
        operand = Container<std::string>(oper, std::greater<std::string>());
        break;
      case ConstraintPredicates::Predicate::GE:
        operand =
            Container<std::string>(oper, std::greater_equal<std::string>());
        break;
      case ConstraintPredicates::Predicate::LE:
        operand = Container<std::string>(oper, std::less_equal<std::string>());
        break;
      case ConstraintPredicates::Predicate::NE:
        operand =
            Container<std::string>(oper, std::not_equal_to<std::string>());
        break;
    }
    if (attribute.at(0) == '?') {
      _attribute = attribute.substr(1);
      _optional = true;
    } else {
      _attribute = attribute;
      _optional = false;
    }
  }

  Constraint(
      std::string attribute, int32_t oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ) {
    switch (op) {
      case ConstraintPredicates::Predicate::EQ:
        operand = Container<int32_t>(oper, std::equal_to<int32_t>());
        break;
      case ConstraintPredicates::Predicate::LT:
        operand = Container<int32_t>(oper, std::less<int32_t>());
        break;
      case ConstraintPredicates::Predicate::GT:
        operand = Container<int32_t>(oper, std::greater<int32_t>());
        break;
      case ConstraintPredicates::Predicate::GE:
        operand = Container<int32_t>(oper, std::greater_equal<int32_t>());
        break;
      case ConstraintPredicates::Predicate::LE:
        operand = Container<int32_t>(oper, std::less_equal<int32_t>());
        break;
      case ConstraintPredicates::Predicate::NE:
        operand = Container<int32_t>(oper, std::not_equal_to<int32_t>());
        break;
    }
    if (attribute.at(0) == '?') {
      _attribute = attribute.substr(1);
      _optional = true;
    } else {
      _attribute = attribute;
      _optional = false;
    }
  }

  Constraint(
      std::string attribute, float oper,
      ConstraintPredicates::Predicate op = ConstraintPredicates::Predicate::EQ) {
    switch (op) {
      case ConstraintPredicates::Predicate::EQ:
        operand = Container<float>(oper, std::equal_to<float>());
        break;
      case ConstraintPredicates::Predicate::LT:
        operand = Container<float>(oper, std::less<float>());
        break;
      case ConstraintPredicates::Predicate::GT:
        operand = Container<float>(oper, std::greater<float>());
        break;
      case ConstraintPredicates::Predicate::GE:
        operand = Container<float>(oper, std::greater_equal<float>());
        break;
      case ConstraintPredicates::Predicate::LE:
        operand = Container<float>(oper, std::less_equal<float>());
        break;
      case ConstraintPredicates::Predicate::NE:
        operand = Container<float>(oper, std::not_equal_to<float>());
        break;
    }
    if (attribute.at(0) == '?') {
      _attribute = attribute.substr(1);
      _optional = true;
    } else {
      _attribute = attribute;
      _optional = false;
    }
  }

  bool operator()(std::string_view input) const {
    if (std::holds_alternative<Container<std::string>>(operand)) {
      return (std::get<Container<std::string>>(operand))
          .operation(std::string(input),
                     std::get<Container<std::string>>(operand).operand);
    } else {
      return false;
    }
  }

  bool operator()(int32_t input) const {
    if (std::holds_alternative<Container<int32_t>>(operand)) {
      return (std::get<Container<int32_t>>(operand))
          .operation(input, std::get<Container<int32_t>>(operand).operand);
    } else {
      return false;
    }
  }

  bool operator()(float input) const {
    if (std::holds_alternative<Container<float>>(operand)) {
      return (std::get<Container<float>>(operand))
          .operation(input, std::get<Container<float>>(operand).operand);
    } else {
      return false;
    }
  }

  bool operator==(const Constraint& other) const {
    return other._attribute == _attribute && other.operand == operand;
  }

  bool optional() const { return _optional; }

  const std::string_view attribute() const {
    return std::string_view(_attribute);
  }

 private:
  std::string _attribute;
  std::variant<std::monostate, Container<std::string>, Container<int32_t>,
               Container<float>>
      operand;
  bool _optional;
};

class Filter {
 public:
  explicit Filter(std::initializer_list<Constraint> constraints) {
    for (Constraint c : constraints) {
      if (c.optional()) {
        optional.push_back(std::move(c));
      } else {
        required.push_back(std::move(c));
      }
    }
  }

  explicit Filter(std::list<Constraint>&& constraints) {
    for (Constraint c : constraints) {
      if (c.optional()) {
        optional.push_back(std::move(c));
      } else {
        required.push_back(std::move(c));
      }
    }
  }

  Filter() = default;

  void clear() {
    required.clear();
    optional.clear();
  }

  bool matches(const Publication& publication) const {
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

  bool operator==(const Filter& other) const {
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

 private:
  std::list<Constraint> required;
  std::list<Constraint> optional;
};

struct MatchedPublication {
  MatchedPublication(Publication&& p, uint32_t subscription_id)
      : publication(p), subscription_id(subscription_id) {}

  MatchedPublication() : publication(), subscription_id(0) {}
  Publication publication;
  uint32_t subscription_id;
};

class SubscriptionHandle;

class Router {
 public:
  static Router& get_instance() {
    static Router instance;
    return instance;
  }

  void publish(Publication&& publication) {
    for (Receiver* receiver : receivers) {
      for (const std::pair<uint32_t, Filter>& filter : receiver->filters) {
        if (filter.second.matches(publication)) {
          receiver->publish(
              MatchedPublication(Publication(publication), filter.first));
        }
      }
    }
  }

  SubscriptionHandle new_subscriber();

 private:
  class Receiver {
   public:
    class Queue;
    explicit Receiver(Router* router);
    ~Receiver();

   private:
    std::optional<MatchedPublication> receive(uint32_t wait_time = 0);
    uint32_t subscribe(Filter&& f) {
      auto it =
          std::find_if(filters.begin(), filters.end(),
                       [&f](const auto& value) { return f == value.second; });
      if (it != filters.end()) {
        return it->first;
      } else {
        uint32_t id = router->next_sub_id++;
        filters.emplace(id, std::move(f));
        return id;
      }
    }

    void unsubscribe(uint32_t sub_id) { filters.erase(sub_id); }

    void publish(MatchedPublication&& publication);

   private:
    Router* router;
    std::map<uint32_t, Filter> filters;
    std::unique_ptr<Queue> queue;
    friend SubscriptionHandle;
    friend Router;
  };

  std::list<Receiver*> receivers;
  std::atomic<uint32_t> next_sub_id{0};

  void deregister_receiver(Receiver* r) { receivers.remove(r); }
  void register_receiver(Receiver* r) { receivers.push_back(r); }
  friend SubscriptionHandle;
};

class SubscriptionHandle {
 public:
  explicit SubscriptionHandle(Router* router)
      : receiver(std::make_unique<Router::Receiver>(router)) {}

  uint32_t subscribe(Filter f) { return receiver->subscribe(std::move(f)); }

  void unsubscribe(uint32_t sub_id) { receiver->unsubscribe(sub_id); }

  std::optional<MatchedPublication> receive(uint32_t wait_time) {
    return receiver->receive(wait_time);
  }

 private:
  std::unique_ptr<Router::Receiver> receiver;
};
}  // namespace PubSub

#endif  //  MAIN_INCLUDE_SUBSCRIPTION_HPP_
