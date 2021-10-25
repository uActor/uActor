#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "board_functions.hpp"
#include "filter.hpp"
#include "matched_publication.hpp"
#include "publication.hpp"
#include "receiver.hpp"
#include "support/string_helper.hpp"

namespace uActor::PubSub {

class Router;

class ReceiverHandle {
 public:
  explicit ReceiverHandle(Router* router)
      : receiver(std::make_unique<Receiver>(router)) {}

  uint32_t subscribe(Filter f, const ActorIdentifier& subscriber,
                     uint8_t priority = 0) {
    return receiver->subscribe(std::move(f), subscriber, priority);
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
}  // namespace uActor::PubSub
