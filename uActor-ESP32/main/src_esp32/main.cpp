#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
extern "C" {
#include <esp_system.h>
}

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "actor_runtime/lua_executor.hpp"
#include "actor_runtime/managed_actor.hpp"
#include "actor_runtime/managed_native_actor.hpp"
#include "actor_runtime/native_executor.hpp"
#include "board_functions.hpp"
#include "lua.hpp"
#include "pubsub/receiver.hpp"
#include "remote/tcp_forwarder.hpp"
#include "remote/wifi_stack.hpp"
#include "support/core_native_actors.hpp"
#include "support/esp32_native_actors.hpp"
#include "support/launch_utils.hpp"
#include "support/testbed.h"

#if CONFIG_ENABLE_BLE_ACTOR
#include "ble_actor.hpp"
#endif

#if CONFIG_ENABLE_GPIO_ACTOR
#include "io/gpio_actor.hpp"
#endif

#if CONFIG_UACTOR_ENABLE_TELEMETRY
#include "controllers/telemetry_actor.hpp"
#include "controllers/telemetry_data.hpp"
#endif

extern "C" {
void app_main(void);
}

#if CONFIG_UACTOR_ENABLE_TELEMETRY
void telemetry_fetch_hook() {
  uActor::Controllers::TelemetryData::set("free_heap", xPortGetFreeHeapSize());
  uActor::Controllers::TelemetryData::set(
      "largest_block", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  uActor::Controllers::TelemetryData::set(
      "heap_general",
      uActor::Support::MemoryManager::total_space[static_cast<size_t>(
          uActor::Support::TrackedRegions::GENERAL)]);

  uActor::Controllers::TelemetryData::set(
      "heap_runtime",
      uActor::Support::MemoryManager::total_space[static_cast<size_t>(
          uActor::Support::TrackedRegions::ACTOR_RUNTIME)]);

  uActor::Controllers::TelemetryData::set(
      "heap_routing",
      uActor::Support::MemoryManager::total_space[static_cast<size_t>(
          uActor::Support::TrackedRegions::ROUTING_STATE)]);

  uActor::Controllers::TelemetryData::set(
      "heap_publications",
      uActor::Support::MemoryManager::total_space[static_cast<size_t>(
          uActor::Support::TrackedRegions::PUBLICATIONS)]);

  uActor::Controllers::TelemetryData::set(
      "heap_debug",
      uActor::Support::MemoryManager::total_space[static_cast<size_t>(
          uActor::Support::TrackedRegions::DEBUG)]);

  uActor::Controllers::TelemetryData::set(
      "active_deployments",
      uActor::Controllers::DeploymentManager::active_deployments());

  uActor::Controllers::TelemetryData::set(
      "inactive_deployments",
      uActor::Controllers::DeploymentManager::inactive_deployments());

  uActor::Controllers::TelemetryData::set(
      "number_of_subscriptions",
      uActor::PubSub::Router::get_instance().number_of_subscriptions());

  uActor::Controllers::TelemetryData::set(
      "total_queue_size", uActor::PubSub::Receiver::total_queue_size.load());
}
#endif

void main_task(void *) {
  uActor::Support::Logger::info("MAIN", "InitialHeap: %d",
                                xPortGetFreeHeapSize());

  uActor::BoardFunctions::setup_hardware();

#if CONFIG_USE_MAC_AS_NODE_ID
  uint8_t mac_buffer[6];
  // TODO(raphaelhetzel) change BoardFunctionsAPI to better support dynamicly
  // set node_ids.
  // This memory is never freed.
  char *mac_hex_buffer = new char[7];
  ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac_buffer));
  snprintf(mac_hex_buffer, 7, "%02X%02X%02X",  // NOLINT
           mac_buffer[3], mac_buffer[4], mac_buffer[5]);
  uActor::BoardFunctions::NODE_ID = mac_hex_buffer;
#else
  uActor::BoardFunctions::NODE_ID = CONFIG_NODE_ID;
#endif

  testbed_log_rt_string("node_id", uActor::BoardFunctions::NODE_ID);

  xTaskCreatePinnedToCore(&uActor::ESP32::Remote::WifiStack::os_task,
                          "WIFI_STACK", 4192, nullptr, 4, nullptr, 0);

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  uActor::Controllers::TelemetryActor::telemetry_fetch_hook =
      telemetry_fetch_hook;
#endif

  uActor::Support::CoreNativeActors::register_native_actors();
  uActor::Support::ESP32NativeActors::register_native_actors();

  uActor::Support::LaunchUtils::await_start_native_executor<BaseType_t>(
      [](uActor::ActorRuntime::ExecutorSettings *params) {
        return xTaskCreatePinnedToCore(
            &uActor::ActorRuntime::NativeExecutor::os_task, "NATIVE_EXECUTOR",
            6168, params, 5, nullptr, 0);
      });

// The E-Paper actor may take seconds, therefore it is isolated to it's own
// executor.
#if CONFIG_ENABLE_EPAPER_DISPLAY
  uActor::Support::LaunchUtils::await_start_native_executor<BaseType_t>(
      [](uActor::ActorRuntime::ExecutorSettings *params) {
        return xTaskCreatePinnedToCore(
            &uActor::ActorRuntime::NativeExecutor::os_task,
            "NATIVE_EXECUTOR_EPAPER", 4096, params, 5, nullptr, 0);
      },
      "epaper");
#endif

  time_t t = 0;
  time(&t);

  size_t retries = 0;
  while (t < 1577836800) {
    uActor::Support::Logger::info("MAIN", "waiting for time");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    time(&t);
    retries++;
  }

  if (t > 1577836800) {
    t -= 1577836800;
    uActor::BoardFunctions::epoch = t;
    uActor::Support::Logger::info("MAIN", "Epoch %ld", t);
  } else {
    uActor::Support::Logger::warning("MAIN", "Epoch not set according to time");
    uActor::BoardFunctions::epoch = 0;
  }

  uActor::Support::LaunchUtils::await_spawn_native_actor("deployment_manager");
  uActor::Support::LaunchUtils::await_spawn_native_actor("topology_manager");
  uActor::Support::LaunchUtils::await_spawn_native_actor("code_store");

  uActor::Support::LaunchUtils::await_start_lua_executor<BaseType_t>(
      [](uActor::ActorRuntime::ExecutorSettings *params) {
        return xTaskCreatePinnedToCore(
            &uActor::ActorRuntime::LuaExecutor::os_task, "LUA_EXECUTOR", 8192,
            params, configMAX_PRIORITIES - 1, nullptr, 1);
      });

#if CONFIG_ENABLE_EPAPER_DISPLAY
  uActor::Support::LaunchUtils::await_spawn_native_actor(
      "epaper_notification_actor", "epaper");
#endif

#if CONFIG_ENABLE_BMP180
  uActor::Support::LaunchUtils::await_spawn_native_actor("bmp180_sensor");
#endif

#if CONFIG_ENABLE_BME280
  uActor::Support::LaunchUtils::await_spawn_native_actor("bme280_sensor");
#endif

#if CONFIG_ENABLE_SCD30
  uActor::Support::LaunchUtils::await_spawn_native_actor("scd30_sensor");
#endif

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  uActor::Support::LaunchUtils::await_spawn_native_actor("telemetry_actor");
#endif

  {
    uActor::PubSub::Publication label_update(uActor::BoardFunctions::NODE_ID,
                                             "root", "1");
    label_update.set_attr("type", "label_update");
    label_update.set_attr("command", "upsert");
    label_update.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    label_update.set_attr("key", "node_id");
    label_update.set_attr("value", uActor::BoardFunctions::NODE_ID);
    uActor::PubSub::Router::get_instance().publish(std::move(label_update));
  }
  for (const auto label : uActor::BoardFunctions::node_labels()) {
    uActor::PubSub::Publication label_update(uActor::BoardFunctions::NODE_ID,
                                             "root", "1");
    label_update.set_attr("type", "label_update");
    label_update.set_attr("command", "upsert");
    label_update.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    label_update.set_attr("key", label.first);
    label_update.set_attr("value", label.second);
    uActor::PubSub::Router::get_instance().publish(std::move(label_update));
  }

#if CONFIG_ENABLE_GPIO_ACTOR
  xTaskCreatePinnedToCore(&uActor::ESP32::IO::GPIOActor::os_task, "GPIO_ACTOR",
                          4192, nullptr, 5, nullptr, 0);
#endif

  auto tcp_task_args =
      uActor::Remote::TCPAddressArguments("0.0.0.0", 1337, "", 0);

  // The TCP forwarder is split into the main task and a select loop.
  // Those could most likely be merged.
  xTaskCreatePinnedToCore(&uActor::Remote::TCPForwarder::os_task, "TCP", 4192,
                          reinterpret_cast<void *>(&tcp_task_args), 4, nullptr,
                          0);
  while (!tcp_task_args.tcp_forwarder) {
    vTaskDelay(100);
  }
  xTaskCreatePinnedToCore(
      &uActor::Remote::TCPForwarder::tcp_reader_task, "TCP2", 6144,
      reinterpret_cast<void *>(tcp_task_args.tcp_forwarder), 4, nullptr, 0);

#if CONFIG_ENABLE_BLE_ACTOR
  xTaskCreatePinnedToCore(&uActor::ESP32::BLE::BLEActor::os_task, "BLE", 4192,
                          nullptr, 4, nullptr, 0);
#endif

  uActor::Support::Logger::info("MAIN", "StaticHeap: %d",
                                xPortGetFreeHeapSize());
  testbed_log_rt_integer("boot_timestamp", t);

  vTaskDelay(2000);
  testbed_log_rt_integer("_ready", t);

  std::string_view raw_static_peers = std::string_view(CONFIG_PERSISTENT_PEERS);
  for (std::string_view raw_static_peer :
       uActor::Support::StringHelper::string_split(raw_static_peers)) {
    uint32_t first_split_pos = raw_static_peer.find_first_of(":");
    uint32_t second_split_pos = raw_static_peer.find_last_of(":");

    std::string_view peer_node_id = raw_static_peer.substr(0, first_split_pos);
    std::string_view peer_ip = raw_static_peer.substr(
        first_split_pos + 1, second_split_pos - first_split_pos - 1);
    std::string_view peer_port = raw_static_peer.substr(second_split_pos + 1);

    uActor::PubSub::Publication add_persistent_peer(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    add_persistent_peer.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    add_persistent_peer.set_attr("type", "add_static_peer");
    add_persistent_peer.set_attr("peer_node_id", peer_node_id);
    add_persistent_peer.set_attr("peer_ip", peer_ip);
    add_persistent_peer.set_attr("peer_port",
                                 std::stoi(std::string(peer_port)));
    uActor::PubSub::Router::get_instance().publish(
        std::move(add_persistent_peer));
  }

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  uActor::Controllers::TelemetryData::increase("inital_heap",
                                               xPortGetFreeHeapSize());
#endif

  vTaskDelete(nullptr);
}

void heap_caps_alloc_failed_hook(size_t requested_size, uint32_t caps,
                                 const char *function_name) {
#if CONFIG_BENCHMARK_ENABLED
  testbed_log_integer("oom", 1);
  testbed_log_integer("_reboot", 1);
#endif
  printf(
      "%s was called but failed to allocate %d bytes with 0x%X capabilities. "
      "\n",
      function_name, requested_size, caps);
#if CONFIG_BENCHMARK_ENABLED
  esp_restart();
#endif
}

esp_err_t error =
    heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook);

void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  xTaskCreatePinnedToCore(&main_task, "MAIN", 4192, nullptr, 5, nullptr, 0);
}
