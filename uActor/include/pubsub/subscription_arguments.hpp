#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "pubsub/publication.hpp"

namespace uActor::PubSub {

enum class Scope : uint8_t { NONE = 0, LOCAL, CLUSTER, GLOBAL };

enum class FetchPolicy : uint8_t { NONE = 0, FETCH, FETCH_FUTURE, FUTURE };

struct SubscriptionArguments {
  Scope scope = Scope::CLUSTER;
  uint32_t priority = 0;
  FetchPolicy fetch_policy = FetchPolicy::FUTURE;

  bool operator==(const SubscriptionArguments& other) const;

  std::shared_ptr<Publication::Map> to_map() const;

  static SubscriptionArguments from_map(const Publication::Map& map);

  static std::string string_from_scope(Scope scope);

  static std::string string_from_fetch_policy(FetchPolicy fetch_policy);

  static Scope scope_from_string(std::string_view scope_string);

  static FetchPolicy fetch_policy_from_string(
      std::string_view fetch_policy_string);
};

}  // namespace uActor::PubSub
