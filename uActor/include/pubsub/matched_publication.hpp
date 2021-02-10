#pragma once

#include <cstdint>
#include <utility>

#include "publication.hpp"

namespace uActor::PubSub {

struct MatchedPublication {
  MatchedPublication(Publication&& p, uint32_t subscription_id)
      : publication(std::move(p)), subscription_id(subscription_id) {}

  MatchedPublication() = default;
  Publication publication{};
  uint32_t subscription_id{0};
};

}  // namespace uActor::PubSub
