#ifndef MAIN_INCLUDE_BOARD_FUNCTIONS_HPP_
#define MAIN_INCLUDE_BOARD_FUNCTIONS_HPP_

#include <cstdint>
#include <list>
#include <string>
#include <utility>

struct BoardFunctions {
  static const uint32_t SLEEP_FOREVER;
  static uint32_t timestamp();
  static void exit_thread();
  static const char* NODE_ID;

  static int32_t epoch;

  static std::list<std::pair<std::string, std::string>> node_labels();
};

#endif  // MAIN_INCLUDE_BOARD_FUNCTIONS_HPP_
