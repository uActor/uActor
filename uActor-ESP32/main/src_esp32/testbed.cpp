#include "support/testbed.h"

#include <esp_timer.h>
#include <sdkconfig.h>

#ifdef CONFIG_TESTBED_NETWORK_UTILS
extern "C" {
#include <esp_netif.h>
}
#endif

#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
#include <string>
#include <utility>
#include <vector>
#endif

namespace testbed {

class TestBed {
 public:
  static constexpr const char* TESTBED_TAG = "TestBed";

  IRAM_ATTR
  static TestBed& get_instance() {
    static TestBed instance;
    return instance;
  }

  TestBed() : sequence_number(0) {
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
    times.reserve(10);
#endif
  }

  IRAM_ATTR
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

  IRAM_ATTR
  void start_timekeeping(size_t variable) {
    timekeeping[variable] = esp_timer_get_time();
  }

#if CONFIG_TESTBED_NESTED_TIMEKEEPING
  IRAM_ATTR
  void stop_timekeeping_inner(size_t variable, const char* name) {
    uint64_t timestamp = esp_timer_get_time();
    times.emplace_back(std::string(name), timestamp - timekeeping[variable]);
  }
#endif

  IRAM_ATTR
  void stop_timekeeping(size_t variable, const char* name) {
    uint64_t timestamp = esp_timer_get_time();

#if CONFIG_TESTBED_NESTED_TIMEKEEPING
    for (const auto t : times) {
      log_integer(t.first.data(), t.second, false);
    }
    times.clear();
#endif

    log_integer(name, timestamp - timekeeping[variable], false);
  }

#ifdef CONFIG_TESTBED_NETWORK_UTILS
  void log_ipv4(const char* variable, esp_ip4_addr_t ip) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", IP2STR(&ip));
    log_generic(variable, "#[Testbed][rt-ipv4][%s][%lld][%ld]:[%s]#\n", buffer);
  }
#endif

 private:
  uint64_t sequence_number;
  std::array<uint64_t, 10> timekeeping;
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
  std::vector<std::pair<std::string, uint64_t>> times;
#endif

  template <class T>
  IRAM_ATTR void log_generic(const char* variable, const char* format,
                             T value) {
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

IRAM_ATTR
void testbed_start_timekeeping(size_t variable) {
  testbed::TestBed::get_instance().start_timekeeping(variable);
}

IRAM_ATTR
void testbed_stop_timekeeping_inner(size_t variable, const char* name) {
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
  testbed::TestBed::get_instance().stop_timekeeping_inner(variable, name);
#endif
}

IRAM_ATTR
void testbed_stop_timekeeping(size_t variable, const char* name) {
  testbed::TestBed::get_instance().stop_timekeeping(variable, name);
}

#ifdef CONFIG_TESTBED_NETWORK_UTILS
void testbed_log_ipv4_address(esp_ip4_addr_t ip) {
  testbed::TestBed::get_instance().log_ipv4("address", ip);
}

void testbed_log_ipv4_netmask(esp_ip4_addr_t ip) {
  testbed::TestBed::get_instance().log_ipv4("netmask", ip);
}

void testbed_log_ipv4_gateway(esp_ip4_addr_t ip) {
  testbed::TestBed::get_instance().log_ipv4("gateway", ip);
}
#endif
