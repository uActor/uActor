#ifndef MAIN_INCLUDE_REMOTE_SEQUENCE_INFO_HPP_
#define MAIN_INCLUDE_REMOTE_SEQUENCE_INFO_HPP_

#include <cstdint>

namespace uActor::Remote {

struct SequenceInfo {
  SequenceInfo(uint32_t sequence_number, uint32_t epoch,
               uint32_t last_timestamp)
      : sequence_number(sequence_number),
        epoch(epoch),
        last_timestamp(last_timestamp) {}

  uint32_t sequence_number;
  uint32_t epoch;
  uint32_t last_timestamp;

  bool is_older_than(uint32_t other_sequence_number, uint32_t other_epoch) {
    return other_epoch > epoch ||
           (other_epoch == epoch && other_sequence_number > sequence_number);
  }
};

}  //  namespace uActor::Remote

#endif  //   MAIN_INCLUDE_REMOTE_SEQUENCE_INFO_HPP_
