#ifndef MAIN_INCLUDE_BOARD_FUNCTIONS_HPP_
#define MAIN_INCLUDE_BOARD_FUNCTIONS_HPP_

#include <cstdint>

struct BoardFunctions {
  static const uint32_t SLEEP_FOREVER;
  static uint32_t timestamp();
  static void exit_thread();
};

#endif  // MAIN_INCLUDE_BOARD_FUNCTIONS_HPP_
