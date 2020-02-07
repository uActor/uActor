#ifndef MAIN_INCLUDE_SUBSCRIPTION_HPP_
#define MAIN_INCLUDE_SUBSCRIPTION_HPP_

#include <testbed.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <variant>

#include "board_functions.hpp"
#include "publication.hpp"
#include "string_helper.hpp"


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

  std::string serialize() {
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

  static std::optional<Filter> deserialize(std::string_view serialized) {
    Filter f;

    if (serialized.length() == 0) {
      return std::move(f);
    }

    size_t optional_index = serialized.find("?");

    std::string_view required_args = serialized.substr(0, optional_index);

    for (std::string_view constraint_string :
         StringHelper::string_split(required_args, "^")) {
      auto constraint = Constraint::deserialize(constraint_string, false);
      if (constraint) {
        f.required.emplace_back(std::move(*constraint));
      } else {
        return std::nullopt;
      }
    }

    if (optional_index != std::string_view::npos) {
      std::string_view optional_args = serialized.substr(optional_index);
      for (std::string_view constraint_string :
           StringHelper::string_split(optional_args, "^")) {
        auto constraint = Constraint::deserialize(constraint_string, true);
        if (constraint) {
          f.optional.emplace_back(std::move(*constraint));
        } else {
          return std::nullopt;
        }
      }
    }

    return std::move(f);
  }

 private:
  std::list<Constraint> required;
  std::list<Constraint> optional;
};

class Router;
class Receiver;

struct Subscription {
  Subscription(uint32_t id, Filter f, std::string node_id, Receiver* r)
      : subscription_id(id), filter(f) {
    nodes.emplace(node_id, std::list<Receiver*>{r});
    receivers.emplace(r, std::list<std::string>{node_id});
  }

  uint32_t subscription_id;
  Filter filter;

  std::unordered_map<std::string, std::list<Receiver*>> nodes;
  std::unordered_map<Receiver*, std::list<std::string>> receivers;
};

struct MatchedPublication {
  MatchedPublication(Publication&& p, uint32_t subscription_id)
      : publication(p), subscription_id(subscription_id) {}

  MatchedPublication() : publication(), subscription_id(0) {}
  Publication publication;
  uint32_t subscription_id;
};

class SubscriptionHandle;

class Receiver {
 public:
  class Queue;
  explicit Receiver(Router* router);
  ~Receiver();

 private:
  std::optional<MatchedPublication> receive(uint32_t wait_time = 0);

  uint32_t subscribe(Filter&& f, std::string node_id = "local");

  void unsubscribe(uint32_t sub_id, std::string node_id = "");

  void publish(MatchedPublication&& publication);

 private:
  Router* router;
  std::unique_ptr<Queue> queue;

  std::set<uint32_t> subscriptions;

  friend SubscriptionHandle;
  friend Router;
};

class Router {
 public:
  static Router& get_instance() {
    static Router instance;
    return instance;
  }

  static void os_task(void*) {
    Router& router = get_instance();
    while (true) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      bool t = true;
      if (router.updated.compare_exchange_weak(t, false)) {
        Publication update{BoardFunctions::NODE_ID, "router", "1"};
        update.set_attr("type", "local_subscription_update");
        update.set_attr("node_id", BoardFunctions::NODE_ID);
        router.publish(std::move(update));
      }
    }
  }

  void publish(Publication&& publication) {
    bool subscriber_found = false;
    testbed_start_timekeeping("publish");
    std::unique_lock lock(mtx);
    for (auto& subscribtion : subscriptions) {
      if (subscribtion.second.filter.matches(publication)) {
        for (auto& receiver : subscribtion.second.receivers) {
          subscriber_found = true;
          receiver.first->publish(
              MatchedPublication(Publication(publication), subscribtion.first));
        }
      }
    }
    testbed_stop_timekeeping("publish");
    if (!subscriber_found) {
      printf("useless message\n");
    }
  }

  SubscriptionHandle new_subscriber();

  std::string subscriptions_for(std::string_view node_id) {
    std::string serialized_sub;
    for (auto& sub : subscriptions) {
      if (sub.second.nodes.size() > 2 ||
          sub.second.nodes.find(std::string(node_id)) ==
              sub.second.nodes.end()) {
        serialized_sub += sub.second.filter.serialize() + "&";
      }
    }
    return serialized_sub;
  }

 private:
  std::atomic<uint32_t> next_sub_id{1};

  std::list<Receiver*> receivers;
  std::map<uint32_t, Subscription> subscriptions;

  std::atomic<bool> updated{false};

  std::recursive_mutex mtx;

  uint32_t add_subscription(Filter&& f, Receiver* r,
                            std::string peer_node_id = std::string("local")) {
    std::unique_lock lck(mtx);
    if (auto it =
            std::find_if(subscriptions.begin(), subscriptions.end(),
                         [&](auto& sub) { return sub.second.filter == f; });
        it != subscriptions.end()) {
      Subscription& sub = it->second;

      if (auto rec_it = sub.receivers.find(r); rec_it != sub.receivers.end()) {
        if (std::find(rec_it->second.begin(), rec_it->second.end(),
                      peer_node_id) == rec_it->second.end()) {
          rec_it->second.push_back(peer_node_id);
        }
      } else {
        sub.receivers.try_emplace(
            r, std::list<std::string>{std::string(peer_node_id)});
      }

      // Check if this subscription is for a new node. A new node will affect
      // the external subscriptions.
      if (auto node_it = sub.nodes.find(peer_node_id);
          node_it != sub.nodes.end()) {
        // Additional receiver: For remote nodes this could mean there is a
        // second path.
        if (std::find(node_it->second.begin(), node_it->second.end(), r) ==
            node_it->second.end()) {
          node_it->second.push_back(r);
        }
      } else {
        if (peer_node_id == "local") {
          publish_subscription_update();
        } else if (sub.nodes.size() == 1) {
          // TODO(raphaelhetzel) Single node update optimization
          // std::string update_node_id = sub.nodes.begin()->first;
          publish_subscription_update();
        }
        sub.nodes.emplace(peer_node_id, std::list<Receiver*>{r});
      }

      return sub.subscription_id;
    } else {  // new subscription
      uint32_t id = next_sub_id++;
      if (peer_node_id == "local") {
        publish_subscription_update();
      } else {
        // TODO(raphaelhetzel) Optimization: We could exclude peer_node_id here.
        publish_subscription_update();
      }
      subscriptions.try_emplace(id, id, std::move(f), std::move(peer_node_id),
                                r);
      return id;
    }
  }

  uint32_t remove_subscription(uint32_t sub_id, Receiver* r,
                               std::string node_id = "") {
    std::unique_lock lck(mtx);
    if (auto sub_it = subscriptions.find(sub_id);
        sub_it != subscriptions.end()) {
      auto& sub = sub_it->second;
      if (auto receiver_it = sub.receivers.find(r);
          receiver_it != sub.receivers.end()) {
        auto& receiver = *receiver_it;

        if (node_id.empty()) {  // Empty node id -> remove subscription for all
                                // node_ids managed by the receiver
          for (auto& sub_node_id : receiver.second) {
            remove_subscription_for_node(&sub, r, sub_node_id);
          }
          sub.receivers.erase(receiver_it);
          return 0;
        } else {
          remove_subscription_for_node(&sub, r, node_id);
          if (receiver.second.size() == 1) {
            sub.receivers.erase(receiver_it);
          } else {
            if (auto node_id_it = std::find(receiver_it->second.begin(),
                                            receiver_it->second.end(), node_id);
                node_id_it != receiver_it->second.end()) {
              receiver_it->second.erase(node_id_it);
            }
          }
          return 0;
        }
      }
    }
    return 0;
  }

  void remove_subscription_for_node(Subscription* sub, Receiver* r,
                                    std::string node_id) {
    if (auto node_it = sub->nodes.find(node_id); node_it != sub->nodes.end()) {
      if (node_it->second.size() == 1) {
        sub->nodes.erase(node_it);
        if (sub->nodes.size() == 0) {
          // TODO(raphaelhetzel) optimization: potentially delay this longer
          // than subscriptions
          publish_subscription_update();
        } else if (sub->nodes.size() == 1) {
          if (node_id != "local") {
            // TODO(raphaelhetzel) optimization: only send to a single node
            publish_subscription_update();
          }
        }
      } else {
        if (auto reference_it =
                std::find(node_it->second.begin(), node_it->second.end(), r);
            reference_it != node_it->second.end()) {
          node_it->second.erase(reference_it);
        }
      }
    }
  }

  void publish_subscription_update() { updated = true; }

  void deregister_receiver(Receiver* r) {
    std::unique_lock lock(mtx);
    receivers.remove(r);
  }
  void register_receiver(Receiver* r) {
    std::unique_lock lock(mtx);
    receivers.push_back(r);
  }
  friend SubscriptionHandle;
  friend Receiver;
};

class SubscriptionHandle {
 public:
  explicit SubscriptionHandle(Router* router)
      : receiver(std::make_unique<Receiver>(router)) {}

  uint32_t subscribe(Filter f,
                     std::string_view node_id = std::string_view("local")) {
    return receiver->subscribe(std::move(f), std::string(node_id));
  }

  void unsubscribe(uint32_t sub_id,
                   std::string_view node_id = std::string_view("")) {
    receiver->unsubscribe(sub_id, std::string(node_id));
  }

  std::optional<MatchedPublication> receive(uint32_t wait_time) {
    return receiver->receive(wait_time);
  }

 private:
  std::unique_ptr<Receiver> receiver;
};
}  // namespace PubSub

#endif  //  MAIN_INCLUDE_SUBSCRIPTION_HPP_
