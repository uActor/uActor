#pragma once

#include "publication.hpp"
#include "types.hpp"

namespace uactor {
// todo currently WASM subscriptions only support a tiny subset of the usual subscription features
class Subscription : private Publication {
 public:
  Subscription() = default;
  void insert(const char* key, const char* value);
  void subscribe() const;
};
}  // namespace uactor
