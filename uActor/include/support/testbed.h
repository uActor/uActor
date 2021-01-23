#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ESP_IDF
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_TESTBED_NETWORK_UTILS
#include <esp_netif.h>
#endif

void testbed_log_integer(const char*, uint64_t);
void testbed_log_double(const char*, double);
void testbed_log_string(const char*, const char*);

void testbed_log_rt_integer(const char*, uint64_t);
void testbed_log_rt_double(const char*, double);
void testbed_log_rt_string(const char*, const char*);

#ifdef CONFIG_TESTBED_NETWORK_UTILS
void testbed_log_ipv4_address(esp_ip4_addr_t ip);
void testbed_log_ipv4_netmask(esp_ip4_addr_t netmask);
void testbed_log_ipv4_gateway(esp_ip4_addr_t gateway);
#endif

void testbed_start_timekeeping(size_t variable);
void testbed_stop_timekeeping(size_t variable, const char* name);

#ifdef CONFIG_TESTBED_NESTED_TIMEKEEPING
void testbed_stop_timekeeping_inner(size_t variable, const char* name);
#endif

#ifdef __cplusplus
}
#endif
