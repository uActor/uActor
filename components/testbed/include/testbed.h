#ifndef COMPONENTS_TESTBED_INCLUDE_TESTBED_H_
#define COMPONENTS_TESTBED_INCLUDE_TESTBED_H_

#ifdef __cplusplus
extern "C" {
#endif

// #include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

#ifdef CONFIG_TESTBED_NETWORK_UTILS
#include <esp_netif.h>
#endif

void testbed_log_integer(const char*, uint64_t, bool runtime_value = false);
void testbed_log_double(const char*, double, bool runtime_value = false);
void testbed_log_string(const char*, const char*, bool runtime_value = false);

#ifdef CONFIG_TESTBED_NETWORK_UTILS
void testbed_log_ipv4_address(esp_ip4_addr_t ip);
void testbed_log_ipv4_netmask(esp_ip4_addr_t netmask);
void testbed_log_ipv4_gateway(esp_ip4_addr_t gateway);
#endif

void testbed_start_timekeeping(size_t variable);
void testbed_stop_timekeeping(size_t variable, const char* name);
void testbed_stop_timekeeping_inner(size_t variable, const char* name);

#ifdef __cplusplus
}
#endif

#endif  //  COMPONENTS_TESTBED_INCLUDE_TESTBED_H_
