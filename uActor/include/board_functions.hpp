#ifndef UACTOR_INCLUDE_BOARD_FUNCTIONS_HPP_
#define UACTOR_INCLUDE_BOARD_FUNCTIONS_HPP_

#include <cstdint>
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace uActor {

struct BoardFunctions {
  static const uint32_t SLEEP_FOREVER;
  static uint32_t timestamp();
  static uint32_t seconds_timestamp();
  static void exit_thread();
  static const char* NODE_ID;
  static std::vector<std::string> SERVER_NODES;

  static int32_t epoch;

  static uint64_t init_time;

  static std::list<std::pair<std::string, std::string>> node_labels();

  static void sleep(uint32_t sleep_ms);

  static void setup_hardware();
};

}  // namespace uActor

#endif  // UACTOR_INCLUDE_BOARD_FUNCTIONS_HPP_
