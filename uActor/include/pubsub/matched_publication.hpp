#ifndef UACTOR_INCLUDE_PUBSUB_MATCHED_PUBLICATION_HPP_
#define UACTOR_INCLUDE_PUBSUB_MATCHED_PUBLICATION_HPP_

#include <cstdint>
#include <utility>

#include "publication.hpp"

namespace uActor::PubSub {

struct MatchedPublication {
  MatchedPublication(Publication&& p, uint32_t subscription_id)
      : publication(std::move(p)), subscription_id(subscription_id) {}

  MatchedPublication() : publication(), subscription_id(0) {}
  Publication publication;
  uint32_t subscription_id;
};

}  // namespace uActor::PubSub

#endif  //  UACTOR_INCLUDE_PUBSUB_MATCHED_PUBLICATION_HPP_
