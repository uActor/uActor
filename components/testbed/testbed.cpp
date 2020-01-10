#include "include/testbed.h"

#include <esp_timer.h>

#ifdef CONFIG_TESTBED_NETWORK_UTILS
extern "C" {
#include <esp_netif.h>
}
#endif

#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace testbed {
struct CStrHasher {
  // We do not want to include std::string here (code size 200k).
  // Hash function taken from
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#public_domain
  static size_t fnv_32a_str(const char* str, size_t hval = 0x811c9dc5) {
    unsigned const char* s = (unsigned const char*)str; /* unsigned string */
    /*
     * FNV-1a hash each octet in the buffer
     */
    while (*s) {
      /* xor the bottom with the current octet */
      hval ^= (size_t)*s++;
      /* multiply by the 32 bit FNV magic prime mod 2^32 */
      hval +=
          (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
    }
    /* return our new hash value */
    return hval;
  }

  size_t operator()(const char* var) const { return fnv_32a_str(var); }
};

struct CStrEquals {
  bool operator()(const char* lhs, const char* rhs) const {
    return strcmp(lhs, rhs) == 0;
  }
};

class TestBed {
 public:
  static constexpr const char* TESTBED_TAG = "TestBed";

  static TestBed& get_instance() {
    static TestBed instance;
    return instance;
  }

  TestBed() : sequence_number(0) { semaphore = xSemaphoreCreateMutex(); }

  void log_integer(const char* variable, int64_t value) {
    log_generic(variable, "#[Testbed][i][%s][%lld][%lld]:[%lld]#\n", value);
  }

  void log_string(const char* variable, const char* value) {
    log_generic(variable, "#[Testbed][s][%s][%lld][%lld]:[%s]#\n", value);
  }

  void log_double(const char* variable, double value) {
    log_generic(variable, "#[Testbed][d][%s][%lld][%lld]:[%lf]#\n", value);
  }

  void start_timekeeping(const char* variable) {
    // std::string requires around 200kb of flash, therefore it is not used here
    char* var = reinterpret_cast<char*>(malloc(strlen(variable) + 1));
    strncpy(var, variable, strlen(variable) + 1);
    timekeeping.emplace(var, esp_timer_get_time());
  }

  void stop_timekeeping(const char* variable) {
    uint64_t timestamp = esp_timer_get_time();
    auto timer = timekeeping.find(variable);
    if (timer != timekeeping.end()) {
      log_integer(variable, timestamp - timer->second);
      delete timer->first;
      timekeeping.erase(timer);
    }
  }

#ifdef CONFIG_TESTBED_NETWORK_UTILS
  void log_ipv4(const char* variable, esp_ip4_addr_t ip) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", IP2STR(&ip));
    log_generic(variable, "#[Testbed][ipv4][%s][%lld][%lld]:[%s]#\n", buffer);
  }
#endif

 private:
  uint64_t sequence_number;
  SemaphoreHandle_t semaphore;
  std::unordered_map<const char*, uint64_t, CStrHasher, CStrEquals> timekeeping;

  template <class T>
  void log_generic(const char* variable, const char* format, T value) {
    uint64_t timestamp = esp_timer_get_time();
    if (xSemaphoreTake(semaphore, portMAX_DELAY)) {
      printf(format, variable, sequence_number++, timestamp, value);
      xSemaphoreGive(semaphore);
    }
  }
};
}  // namespace testbed

void testbed_log_integer(const char* variable, uint64_t value) {
  testbed::TestBed::get_instance().log_integer(variable, value);
}

void testbed_log_string(const char* variable, const char* value) {
  testbed::TestBed::get_instance().log_string(variable, value);
}

void testbed_log_double(const char* variable, double value) {
  testbed::TestBed::get_instance().log_double(variable, value);
}

void testbed_start_timekeeping(const char* variable) {
  testbed::TestBed::get_instance().start_timekeeping(variable);
}

void testbed_stop_timekeeping(const char* variable) {
  testbed::TestBed::get_instance().stop_timekeeping(variable);
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
