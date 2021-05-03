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

#include "actor_runtime/code_store_actor.hpp"
#include "actor_runtime/lua_executor.hpp"
#include "actor_runtime/managed_actor.hpp"
#include "actor_runtime/managed_native_actor.hpp"
#include "actor_runtime/native_executor.hpp"
#include "board_functions.hpp"
#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"
#include "io/gpio_actor.hpp"
#include "lua.hpp"
#include "pubsub/receiver.hpp"
#include "remote/tcp_forwarder.hpp"
#include "remote/wifi_stack.hpp"
#include "support/testbed.h"

#if CONFIG_ENABLE_BMP180
#include "bmp180_actor.hpp"
#endif
#if CONFIG_ENABLE_BME280
#include "bme280_actor.hpp"
#endif
#if CONFIG_ENABLE_SCD30
#include "scd30_actor.hpp"
#endif
// TODO(raphaelhetzel) this currently required patching callEPD
#if CONFIG_ENABLE_EPAPER_DISPLAY
#include "epaper_actor.hpp"
#endif

#if CONFIG_ENABLE_BLE_ACTOR
#include "ble_actor.hpp"
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
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());

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

  uActor::ActorRuntime::ExecutorSettings executor_settings = {
      .node_id = uActor::BoardFunctions::NODE_ID, .instance_id = "1"};

  xTaskCreatePinnedToCore(&uActor::ESP32::Remote::WifiStack::os_task,
                          "WIFI_STACK", 4192, nullptr, 4, nullptr, 0);

  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Controllers::TopologyManager>("topology_manager");
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Controllers::DeploymentManager>("deployment_manager");
#if CONFIG_ENABLE_BMP180
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ESP32::IO::BMP180Actor>("bmp180_sensor");
#endif
#if CONFIG_ENABLE_BME280
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ESP32::Sensors::BME280Actor>("bme280_sensor");
#endif
#if CONFIG_ENABLE_SCD30
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ESP32::Sensors::SCD30Actor>("scd30_sensor");
#endif
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ActorRuntime::CodeStoreActor>("code_store");

#if CONFIG_ENABLE_EPAPER_DISPLAY
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ESP32::Notifications::EPaperNotificationActor>(
      "epaper_notification_actor");
#endif

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  uActor::Controllers::TelemetryActor::telemetry_fetch_hook =
      telemetry_fetch_hook;
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Controllers::TelemetryActor>("telemetry_actor");
#endif

  xTaskCreatePinnedToCore(&uActor::ActorRuntime::NativeExecutor::os_task,
                          "NATIVE_EXECUTOR", 6168, &executor_settings, 5,
                          nullptr, 0);

  time_t t = 0;
  time(&t);

  size_t retries = 0;
  while (t < 1577836800) {
    printf("waiting for time\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    time(&t);
    retries++;
  }

  if (t > 1577836800) {
    t -= 1577836800;
    uActor::BoardFunctions::epoch = t;
    printf("epoch %ld\n", t);
  } else {
    printf("Epoch not set according to time\n");
    uActor::BoardFunctions::epoch = 0;
  }

  vTaskDelay(2000 / portTICK_PERIOD_MS);
  {
    auto create_deployment_manager = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    create_deployment_manager.set_attr("command", "spawn_native_actor");
    create_deployment_manager.set_attr("spawn_actor_version", "default");
    create_deployment_manager.set_attr("spawn_node_id",
                                       uActor::BoardFunctions::NODE_ID);
    create_deployment_manager.set_attr("spawn_actor_type",
                                       "deployment_manager");
    create_deployment_manager.set_attr("spawn_instance_id", "1");
    create_deployment_manager.set_attr("node_id",
                                       uActor::BoardFunctions::NODE_ID);
    create_deployment_manager.set_attr("actor_type", "native_executor");
    create_deployment_manager.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(
        std::move(create_deployment_manager));
  }
  {
    auto create_topology_manager = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    create_topology_manager.set_attr("command", "spawn_native_actor");
    create_topology_manager.set_attr("spawn_actor_version", "default");
    create_topology_manager.set_attr("spawn_node_id",
                                     uActor::BoardFunctions::NODE_ID);
    create_topology_manager.set_attr("spawn_actor_type", "topology_manager");
    create_topology_manager.set_attr("spawn_instance_id", "1");
    create_topology_manager.set_attr("node_id",
                                     uActor::BoardFunctions::NODE_ID);
    create_topology_manager.set_attr("actor_type", "native_executor");
    create_topology_manager.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(
        std::move(create_topology_manager));
  }

#if CONFIG_ENABLE_EPAPER_DISPLAY
  {
    auto create_display = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    create_display.set_attr("command", "spawn_native_actor");
    create_display.set_attr("spawn_code", "");
    create_display.set_attr("spawn_node_id", uActor::BoardFunctions::NODE_ID);
    create_display.set_attr("spawn_actor_type", "epaper_notification_actor");
    create_display.set_attr("spawn_actor_version", "default");
    create_display.set_attr("spawn_instance_id", "1");
    create_display.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    create_display.set_attr("actor_type", "native_executor");
    create_display.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(std::move(create_display));
  }
#endif

  {
    auto create_code_store = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    create_code_store.set_attr("command", "spawn_native_actor");
    create_code_store.set_attr("spawn_actor_version", "default");
    create_code_store.set_attr("spawn_node_id",
                               uActor::BoardFunctions::NODE_ID);
    create_code_store.set_attr("spawn_actor_type", "code_store");
    create_code_store.set_attr("spawn_instance_id", "1");
    create_code_store.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    create_code_store.set_attr("actor_type", "native_executor");
    create_code_store.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(
        std::move(create_code_store));
  }

#if CONFIG_ENABLE_BMP180
  {
    auto create_bmp180_sensor = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    create_bmp180_sensor.set_attr("command", "spawn_native_actor");
    create_bmp180_sensor.set_attr("spawn_code", "");
    create_bmp180_sensor.set_attr("spawn_node_id",
                                  uActor::BoardFunctions::NODE_ID);
    create_bmp180_sensor.set_attr("spawn_actor_version", "default");
    create_bmp180_sensor.set_attr("spawn_actor_type", "bmp180_sensor");
    create_bmp180_sensor.set_attr("spawn_instance_id", "1");
    create_bmp180_sensor.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    create_bmp180_sensor.set_attr("actor_type", "native_executor");
    create_bmp180_sensor.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(
        std::move(create_bmp180_sensor));
  }
#endif

#if CONFIG_ENABLE_BME280
  {
    auto create_bme280_sensor = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    create_bme280_sensor.set_attr("command", "spawn_native_actor");
    create_bme280_sensor.set_attr("spawn_code", "");
    create_bme280_sensor.set_attr("spawn_node_id",
                                  uActor::BoardFunctions::NODE_ID);
    create_bme280_sensor.set_attr("spawn_actor_version", "default");
    create_bme280_sensor.set_attr("spawn_actor_type", "bme280_sensor");
    create_bme280_sensor.set_attr("spawn_instance_id", "1");
    create_bme280_sensor.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    create_bme280_sensor.set_attr("actor_type", "native_executor");
    create_bme280_sensor.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(
        std::move(create_bme280_sensor));
  }
#endif

#if CONFIG_ENABLE_SCD30
  {
    auto create_scd30_sensor = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    create_scd30_sensor.set_attr("command", "spawn_native_actor");
    create_scd30_sensor.set_attr("spawn_code", "");
    create_scd30_sensor.set_attr("spawn_node_id",
                                 uActor::BoardFunctions::NODE_ID);
    create_scd30_sensor.set_attr("spawn_actor_type", "scd30_sensor");
    create_scd30_sensor.set_attr("spawn_instance_id", "1");
    create_scd30_sensor.set_attr("spawn_actor_version", "default");
    create_scd30_sensor.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    create_scd30_sensor.set_attr("actor_type", "native_executor");
    create_scd30_sensor.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(
        std::move(create_scd30_sensor));
  }
#endif

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  {
    auto create_telemetry_actor = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    create_telemetry_actor.set_attr("command", "spawn_native_actor");
    create_telemetry_actor.set_attr("spawn_code", "");
    create_telemetry_actor.set_attr("spawn_node_id",
                                    uActor::BoardFunctions::NODE_ID);
    create_telemetry_actor.set_attr("spawn_actor_type", "telemetry_actor");
    create_telemetry_actor.set_attr("spawn_actor_version", "default");
    create_telemetry_actor.set_attr("spawn_instance_id", "1");
    create_telemetry_actor.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    create_telemetry_actor.set_attr("actor_type", "native_executor");
    create_telemetry_actor.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(
        std::move(create_telemetry_actor));
  }
#endif

  vTaskDelay(50 / portTICK_PERIOD_MS);
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

  xTaskCreatePinnedToCore(&uActor::ActorRuntime::LuaExecutor::os_task,
                          "LUA_EXECUTOR", 8192, &executor_settings,
                          configMAX_PRIORITIES - 1, nullptr, 1);

  xTaskCreatePinnedToCore(&uActor::ESP32::IO::GPIOActor::os_task, "GPIO_ACTOR",
                          4192, nullptr, 5, nullptr, 0);

  auto tcp_task_args =
      uActor::Remote::TCPAddressArguments("0.0.0.0", 1337, "", 0);

  xTaskCreatePinnedToCore(&uActor::Remote::TCPForwarder::os_task, "TCP", 4192,
                          reinterpret_cast<void *>(&tcp_task_args), 4, nullptr,
                          0);

  while (!tcp_task_args.tcp_forwarder) {
    vTaskDelay(1000);
  }

  xTaskCreatePinnedToCore(
      &uActor::Remote::TCPForwarder::tcp_reader_task, "TCP2", 6144,
      reinterpret_cast<void *>(tcp_task_args.tcp_forwarder), 4, nullptr, 0);

#if CONFIG_ENABLE_BLE_ACTOR
  xTaskCreatePinnedToCore(&uActor::ESP32::BLE::BLEActor::os_task, "BLE", 4192,
                          nullptr, 4, nullptr, 0);
#endif

  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
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

  vTaskDelete(nullptr);
}

void heap_caps_alloc_failed_hook(size_t requested_size, uint32_t caps,
                                 const char *function_name) {
  printf(
      "%s was called but failed to allocate %d bytes with 0x%X capabilities. "
      "\n",
      function_name, requested_size, caps);
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
