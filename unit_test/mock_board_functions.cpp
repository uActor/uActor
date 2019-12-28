#include <cstdint>
#include <ctime>

#include "board_functions.hpp"

const uint32_t BoardFunctions::SLEEP_FOREVER = UINT32_MAX;

uint32_t BoardFunctions::timestamp() {
  return static_cast<uint32_t>(std::time(nullptr));
}

void BoardFunctions::exit_thread() {}
