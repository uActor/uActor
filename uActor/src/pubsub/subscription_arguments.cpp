#include "pubsub/subscription_arguments.hpp"

namespace uActor::PubSub {
bool SubscriptionArguments::operator==(
    const SubscriptionArguments& other) const {
  return other.scope == scope && other.priority == priority &&
         other.fetch_policy == fetch_policy;
}

std::shared_ptr<Publication::Map> SubscriptionArguments::to_map() const {
  auto output = std::make_shared<Publication::Map>();
  output->set_attr("priority", static_cast<int32_t>(priority));
  output->set_attr("fetch_policy", string_from_fetch_policy(fetch_policy));
  output->set_attr("scope", string_from_scope(scope));
  return output;
}

SubscriptionArguments SubscriptionArguments::from_map(
    const Publication::Map& map) {
  SubscriptionArguments s;

  auto prio = map.get_attr("priority");
  if (std::holds_alternative<int32_t>(prio)) {
    s.priority = static_cast<uint32_t>(std::get<int32_t>(prio));
  }

  auto fetch_policy = map.get_attr("fetch_policy");
  if (std::holds_alternative<std::string_view>(fetch_policy)) {
    s.fetch_policy =
        fetch_policy_from_string(std::get<std::string_view>(fetch_policy));
  }

  auto scope = map.get_attr("scope");
  if (std::holds_alternative<std::string_view>(scope)) {
    s.scope = scope_from_string(std::get<std::string_view>(scope));
  }

  return s;
}

std::string SubscriptionArguments::string_from_scope(Scope scope) {
  switch (scope) {
    case Scope::LOCAL:
      return "LOCAL";
    case Scope::CLUSTER:
      return "CLUSTER";
    case Scope::GLOBAL:
      return "GLOBAL";
    default:
      return "NONE";
  }
}

std::string SubscriptionArguments::string_from_fetch_policy(
    FetchPolicy fetch_policy) {
  switch (fetch_policy) {
    case FetchPolicy::FETCH:
      return "FETCH";
    case FetchPolicy::FETCH_FUTURE:
      return "FETCH_FUTURE";
    case FetchPolicy::FUTURE:
      return "FUTURE";
    default:
      return "NONE";
  }
}

Scope SubscriptionArguments::scope_from_string(std::string_view scope_string) {
  if (scope_string == "LOCAL") {
    return Scope::LOCAL;
  } else if (scope_string == "CLUSTER") {
    return Scope::CLUSTER;
  } else if (scope_string == "GLOBAL") {
    return Scope::GLOBAL;
  } else {
    return Scope::NONE;
  }
}

FetchPolicy SubscriptionArguments::fetch_policy_from_string(
    std::string_view fetch_policy_string) {
  if (fetch_policy_string == "FETCH") {
    return FetchPolicy::FETCH;
  } else if (fetch_policy_string == "FETCH_FUTURE") {
    return FetchPolicy::FETCH_FUTURE;
  } else if (fetch_policy_string == "FUTURE") {
    return FetchPolicy::FUTURE;
  } else {
    return FetchPolicy::NONE;
  }
}
}  // namespace uActor::PubSub
