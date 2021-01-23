#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

#include "board_functions.hpp"
#include "forwarding_strategy.hpp"
#include "pubsub/publication.hpp"
#include "sequence_info.hpp"
#include "support/logger.hpp"

namespace uActor::Remote {

class SequenceNumberForwardingStrategy : public ForwardingStrategy {
 public:
  // Incomming
  bool should_accept(const PubSub::Publication& p);
  void add_incomming_routing_fields(PubSub::Publication* p);

  // Outgoing
  bool should_forward(const PubSub::Publication& p);
  // TODO(raphaelhetzel) move the addition of the sequence information to this
  // class
  void add_outgoing_routing_fields(PubSub::Publication* p) {}

  ~SequenceNumberForwardingStrategy() {}

  static std::atomic<uint32_t> sequence_number;

 private:
  std::unordered_map<std::string, Remote::SequenceInfo> outgoing_sequence_infos;
  static std::unordered_map<std::string, Remote::SequenceInfo>
      incomming_sequence_infos;
  static std::mutex incomming_mtx;
};

}  // namespace uActor::Remote
