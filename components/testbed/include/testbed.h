#ifndef COMPONENTS_TESTBED_INCLUDE_TESTBED_H_
#define COMPONENTS_TESTBED_INCLUDE_TESTBED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

#ifdef CONFIG_TESTBED_NETWORK_UTILS
#include <esp_netif.h>
#endif

void testbed_log_integer(const char*, uint64_t);
void testbed_log_double(const char*, double);
void testbed_log_string(const char*, const char*);

#ifdef CONFIG_TESTBED_NETWORK_UTILS
void testbed_log_ipv4_address(esp_ip4_addr_t ip);
void testbed_log_ipv4_netmask(esp_ip4_addr_t netmask);
void testbed_log_ipv4_gateway(esp_ip4_addr_t gateway);
#endif

void testbed_start_timekeeping(const char*);
void testbed_stop_timekeeping(const char*);

#ifdef __cplusplus
}
#else
// As there unfortunately isn't a C++ alternative to this yet, this is not
// discussed in the documentation and might change in the future.
#define testbed_log(var, val) \
  _Generic((val), \
  int: testbed_log_integer, \
  double: testbed_log_double, \
  char*: testbed_log_string)(var, val)
#endif

#endif  //  COMPONENTS_TESTBED_INCLUDE_TESTBED_H_
