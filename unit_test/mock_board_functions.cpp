#include <chrono>
#include <cstdint>

#include "board_functions.hpp"

const uint32_t BoardFunctions::SLEEP_FOREVER = UINT32_MAX;

uint32_t BoardFunctions::timestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void BoardFunctions::exit_thread() {}
