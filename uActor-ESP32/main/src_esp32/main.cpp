// TODO(raphaelhetzel) Split into multiple files once the APIs are defined

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
#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"
#include "io/gpio_actor.hpp"
#include "lua.hpp"
#include "pubsub/router.hpp"
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

extern "C" {
void app_main(void);
}

void main_task(void *) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());

  uActor::BoardFunctions::setup_hardware();

  xTaskCreatePinnedToCore(&uActor::PubSub::Router::os_task, "Router", 4192,
                          nullptr, 3, nullptr, 0);

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
      uActor::ActorRuntime::CodeStore>("code_store");

#if CONFIG_ENABLE_EPAPER_DISPLAY
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ESP32::Notifications::EPaperNotificationActor>(
      "epaper_notification_actor");
#endif

  xTaskCreatePinnedToCore(&uActor::ActorRuntime::NativeExecutor::os_task,
                          "NATIVE_EXECUTOR", 6168, &executor_settings, 5,
                          nullptr, 0);

  time_t t = 0;
  time(&t);

  size_t retries = 0;
  while (t < 1577836800 && retries < 10) {
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
  auto create_deployment_manager =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
  create_deployment_manager.set_attr("command", "spawn_native_actor");
  create_deployment_manager.set_attr("spawn_actor_version", "default");
  create_deployment_manager.set_attr("spawn_node_id",
                                     uActor::BoardFunctions::NODE_ID);
  create_deployment_manager.set_attr("spawn_actor_type", "deployment_manager");
  create_deployment_manager.set_attr("spawn_instance_id", "1");
  create_deployment_manager.set_attr("node_id",
                                     uActor::BoardFunctions::NODE_ID);
  create_deployment_manager.set_attr("actor_type", "native_executor");
  create_deployment_manager.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(
      std::move(create_deployment_manager));

  auto create_topology_manager =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
  create_topology_manager.set_attr("command", "spawn_native_actor");
  create_topology_manager.set_attr("spawn_actor_version", "default");
  create_topology_manager.set_attr("spawn_node_id",
                                   uActor::BoardFunctions::NODE_ID);
  create_topology_manager.set_attr("spawn_actor_type", "topology_manager");
  create_topology_manager.set_attr("spawn_instance_id", "1");
  create_topology_manager.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
  create_topology_manager.set_attr("actor_type", "native_executor");
  create_topology_manager.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(
      std::move(create_topology_manager));

  auto create_code_store =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
  create_code_store.set_attr("command", "spawn_native_actor");
  create_code_store.set_attr("spawn_actor_version", "default");
  create_code_store.set_attr("spawn_node_id", uActor::BoardFunctions::NODE_ID);
  create_code_store.set_attr("spawn_actor_type", "code_store");
  create_code_store.set_attr("spawn_instance_id", "1");
  create_code_store.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
  create_code_store.set_attr("actor_type", "native_executor");
  create_code_store.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(std::move(create_code_store));

#if CONFIG_ENABLE_BMP180
  auto create_bmp180_sensor =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
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
#endif

#if CONFIG_ENABLE_BME280
  auto create_bme280_sensor =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
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
#endif

#if CONFIG_ENABLE_SCD30
  auto create_scd30_sensor =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
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
#endif

#if CONFIG_ENABLE_EPAPER_DISPLAY
  auto create_display =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
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
      &uActor::Remote::TCPForwarder::tcp_reader_task, "TCP2", 4192,
      reinterpret_cast<void *>(tcp_task_args.tcp_forwarder), 4, nullptr, 0);

#if CONFIG_ENABLE_BLE_ACTOR
  xTaskCreatePinnedToCore(&uActor::ESP32::BLE::BLEActor::os_task, "BLE",
  4192,
                          nullptr, 4, nullptr, 0);
#endif

  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  testbed_log_rt_integer("boot_timestamp", t);

  vTaskDelay(2000);
  testbed_log_rt_integer("_ready", t);

  vTaskDelete(nullptr);

  // while (true) {
  //   vTaskDelay(5000 / portTICK_PERIOD_MS);
  //   printf("Heap: %d \n", xPortGetFreeHeapSize());
  // }
}

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
