#ifndef MAIN_LEGACY_BENCHMARK_ACTOR_HPP_
#define MAIN_LEGACY_BENCHMARK_ACTOR_HPP_

#include <utility>

#include "benchmark_configuration.hpp"
#include "include/message.hpp"
#include "include/router_v2.hpp"

class Actor {
 public:
  explicit Actor(uint64_t id) : id(id) {
    timestamp = xTaskGetTickCount();
    round = 0;
    snprintf(self, sizeof(self), "%lld", id);
    snprintf(next, sizeof(next), "%lld", (id + 1) % 32);
  }

  void receive(uint64_t sender, char* message) {
    if (round == 0) {
      timestamp = xTaskGetTickCount();
    }
    if (round == 10 && id == 0) {
      printf("Measurement: %d\n",
             portTICK_PERIOD_MS * (xTaskGetTickCount() - timestamp));
      printf("IterationHeap: %d \n", xPortGetFreeHeapSize());
      fflush(stdout);
      round = 0;
      timestamp = xTaskGetTickCount();
    }
    if (id == 0) {
      round++;
    }

    Message m = Message(self, next, 1234, STATIC_MESSAGE_SIZE);
#if TOUCH_DATA
#if TOUCH_BYTEWISE
    for (int i = 0; i < STATIC_MESSAGE_SIZE; i++) {
      if ((i % (STATIC_MESSAGE_SIZE / 20) == 0)) {
        *(const_cast<char*>(m.buffer()) + i) = 50;
      } else {
        *(const_cast<char*>(m.buffer()) + i) = 49;
      }
    }
#else
    uint32_t* casted_buffer =
        reinterpret_cast<uint32_t*>(const_cast<char*>(m.buffer()));
    for (int i = 0; i < STATIC_MESSAGE_SIZE / 4; i++) {
      if ((i * 4 % (STATIC_MESSAGE_SIZE / 20) == 0)) {
        *(casted_buffer + i) = 49 << 24 | 49 << 16 | 49 << 8 | 50;
      } else if ((i * 4 + 1) % (STATIC_MESSAGE_SIZE / 20) == 0) {
        *(casted_buffer + i) = 49 << 24 | 49 << 16 | 50 << 8 | 49;
      } else if ((i * 4 + 2) % (STATIC_MESSAGE_SIZE / 20) == 0) {
        *(casted_buffer + i) = 49 << 24 | 50 << 16 | 49 << 8 | 49;
      } else if ((i * 4 + 3) % (STATIC_MESSAGE_SIZE / 20) == 0) {
        *(casted_buffer + i) = 50 << 24 | 49 << 16 | 49 << 8 | 49;
      } else {
        *(casted_buffer + i) = 49 << 24 | 49 << 16 | 49 << 8 | 49;
      }
    }
#endif
#endif
    RouterV2::getInstance().send(std::move(m));
  }

 private:
  char self[128], next[128];
  uint64_t id;
  uint32_t timestamp;
  uint32_t round;
};

#endif  // MAIN_LEGACY_BENCHMARK_ACTOR_HPP_
