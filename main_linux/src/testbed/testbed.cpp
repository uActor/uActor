// TODO(raphaelhetzel) unify with the ESP32 version

#include "testbed.h"

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif

#include <array>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <cstdint>
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
#include <vector>
#include <string>
#include <utility>
#endif

namespace testbed {

class TestBed {
 public:
  static constexpr const char* TESTBED_TAG = "TestBed";

  static TestBed& get_instance() {
    static TestBed instance;
    return instance;
  }

  TestBed() : sequence_number(0) {
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
    times.reserve(10);
#endif
  }

  void log_integer(const char* variable, int64_t value,
                   bool runtime_value = false) {
    if (runtime_value) {
      log_generic(variable, "#[Testbed][rt-i][%s][%lld][%ld]:[%lld]#\n", value);
    } else {
      log_generic(variable, "#[Testbed][i][%s][%lld][%ld]:[%lld]#\n", value);
    }
  }

  void log_string(const char* variable, const char* value,
                  bool runtime_value = false) {
    if (runtime_value) {
      log_generic(variable, "#[Testbed][rt-s][%s][%lld][%ld]:[%s]#\n", value);
    } else {
      log_generic(variable, "#[Testbed][s][%s][%lld][%ld]:[%s]#\n", value);
    }
  }

  void log_double(const char* variable, double value,
                  bool runtime_value = false) {
    if (runtime_value) {
      log_generic(variable, "#[Testbed][rt-d][%s][%lld][%ld]:[%lf]#\n", value);
    } else {
      log_generic(variable, "#[Testbed][d][%s][%lld][%ld]:[%lf]#\n", value);
    }
  }

  void start_timekeeping(size_t variable) {
    timekeeping[variable] = std::chrono::high_resolution_clock::now();
  }

#if CONFIG_TESTBED_NESTED_TIMEKEEPING
  void stop_timekeeping_inner(size_t variable, const char* name) {
    uint64_t timestamp = std::chrono::high_resolution_clock::now();
    times.emplace_back(std::string(name), std::chrono::duration_cast<std::chrono::microseconds>(timestamp - timekeeping[variable]).count());
  }
#endif

  void stop_timekeeping(size_t variable, const char* name) {
    auto timestamp = std::chrono::high_resolution_clock::now();

#if CONFIG_TESTBED_NESTED_TIMEKEEPING
  for (const auto t : times) {
    log_integer(t.first.data(), t.second, false);
  }
  times.clear();
#endif

    log_integer(name, std::chrono::duration_cast<std::chrono::microseconds>(timestamp - timekeeping[variable]).count(), false);
  }

 private:
uint64_t sequence_number;
  std::array<std::chrono::time_point<std::chrono::high_resolution_clock>, 10> timekeeping;
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
  std::vector<std::pair<std::string, uint64_t>> times;
#endif

  template <class T>
  void log_generic(const char* variable, const char* format, T value) {
    // time_t timestamp;
    // time(&timestamp);
    printf(format, variable, sequence_number++, 0, value);
  }
};
}  // namespace testbed

void testbed_log_integer(const char* variable, uint64_t value) {
  testbed::TestBed::get_instance().log_integer(variable, value, false);
}

void testbed_log_string(const char* variable, const char* value) {
  testbed::TestBed::get_instance().log_string(variable, value, false);
}

void testbed_log_double(const char* variable, double value) {
  testbed::TestBed::get_instance().log_double(variable, value, false);
}

void testbed_log_rt_integer(const char* variable, uint64_t value) {
  testbed::TestBed::get_instance().log_integer(variable, value, true);
}

void testbed_log_rt_string(const char* variable, const char* value) {
  testbed::TestBed::get_instance().log_string(variable, value, true);
}

void testbed_log_rt_double(const char* variable, double value) {
  testbed::TestBed::get_instance().log_double(variable, value, true);
}

void testbed_start_timekeeping(size_t variable) {
  testbed::TestBed::get_instance().start_timekeeping(variable);
}
void testbed_stop_timekeeping_inner(size_t variable, const char* name) {
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
  testbed::TestBed::get_instance().stop_timekeeping_inner(variable, name);
#endif
}

void testbed_stop_timekeeping(size_t variable, const char* name) {
  testbed::TestBed::get_instance().stop_timekeeping(variable, name);
}
