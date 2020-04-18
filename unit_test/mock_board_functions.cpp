#include <chrono>
#include <cstdint>
#include <thread>

#include "board_functions.hpp"

namespace uActor {

const uint32_t BoardFunctions::SLEEP_FOREVER = UINT32_MAX;

uint32_t BoardFunctions::timestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void BoardFunctions::exit_thread() {}

const char* BoardFunctions::NODE_ID = "node_1";

void BoardFunctions::sleep(uint32_t sleep_ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}

int32_t BoardFunctions::epoch = 0;

}  // namespace uActor
