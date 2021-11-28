#include "remote/stream_publication_framer.hpp"

extern "C" {
#include <arpa/inet.h>
}

#include <cassert>
#include <cstring>

namespace uActor::Remote {

void StreamPublicationFramer::process_data(
    uint32_t len, char* data,
    std::function<void(PubSub::Publication&& publication,
                       size_t encoded_length)>
        complete_callback) {
  uint32_t bytes_remaining = len;
  while (bytes_remaining > 0) {
    if (state == empty) {
      if (bytes_remaining >= 4) {
        publicaton_full_size =
            ntohl(*reinterpret_cast<uint32_t*>(data + (len - bytes_remaining)));
        publicaton_remaining_bytes = publicaton_full_size;
        bytes_remaining -= 4;
        state = waiting_for_data;
      } else {
        std::memcpy(size_buffer, data + (len - bytes_remaining),
                    bytes_remaining);
        size_field_remaining_bytes = 4 - bytes_remaining;
        bytes_remaining = 0;
        state = waiting_for_size;
      }
    } else if (state == waiting_for_size) {
      if (bytes_remaining > size_field_remaining_bytes) {
        std::memcpy(size_buffer + (4 - size_field_remaining_bytes),
                    data + (len - bytes_remaining), size_field_remaining_bytes);
        publicaton_full_size = ntohl(*reinterpret_cast<uint32_t*>(size_buffer));
        publicaton_remaining_bytes = publicaton_full_size;
        bytes_remaining -= size_field_remaining_bytes;
        size_field_remaining_bytes = 0;
        state = waiting_for_data;
      } else {
        std::memcpy(size_buffer + (4 - size_field_remaining_bytes),
                    data + (len - bytes_remaining), bytes_remaining);
        size_field_remaining_bytes =
            size_field_remaining_bytes - bytes_remaining;
        bytes_remaining = 0;
      }
    } else if (state == waiting_for_data) {
      uint32_t to_move = std::min(publicaton_remaining_bytes, bytes_remaining);
      publication_buffer.write(data + (len - bytes_remaining), to_move);
      bytes_remaining -= to_move;
      publicaton_remaining_bytes -= to_move;
      if (publicaton_remaining_bytes == 0) {
#if CONFIG_BENCHMARK_BREAKDOWN
        timeval tv;
        gettimeofday(&tv, NULL);
#endif
        std::optional<PubSub::Publication> p;
        if (publicaton_full_size > 0) {
          p = publication_buffer.build();
        }
        if (p) {
          complete_callback(std::move(*p), publicaton_full_size);
        }
        publication_buffer = PubSub::PublicationFactory();
        state = empty;
        assert(publicaton_remaining_bytes == 0);
      }
    }
  }
}

}  // namespace uActor::Remote
