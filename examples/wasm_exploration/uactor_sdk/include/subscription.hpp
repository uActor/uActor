#pragma once

#include "publication.hpp"
#include "types.hpp"

namespace uactor {

class Subscription : private Publication {
 public:
  Subscription() = default;
  void insert(const char* key, const char* value);
  void subscribe() const;
};
}  // namespace uactor
