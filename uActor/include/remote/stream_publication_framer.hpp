#pragma once

#include <cstdint>
#include <functional>

#include "pubsub/publication_factory.hpp"

namespace uActor::Remote {

// Extract messages from a stream (e.g. TCP) containing pairs of (length
// (uint32_t,network byte order), content (Msgpack))
// https://blog.stephencleary.com/2009/04/message-framing.html
struct StreamPublicationFramer {
  enum ProcessingState {
    empty,
    waiting_for_size,
    waiting_for_data,
  };

  // Connection statemachine
  ProcessingState state{empty};

  // The size field may be split into multiple recvs
  char size_buffer[4]{0, 0, 0, 0};
  uint32_t size_field_remaining_bytes{0};

  uint32_t publicaton_remaining_bytes{0};
  uint32_t publicaton_full_size{0};
  PubSub::PublicationFactory publication_buffer = PubSub::PublicationFactory();

  void process_data(uint32_t len, char* data,
                    std::function<void(PubSub::Publication&& publication,
                                       size_t encoded_length)>
                        complete_callback);

  // TODO(raphaelhetzel) Move the helpers for the reverse direction
  // (publication->bytes)
};

}  // namespace uActor::Remote
