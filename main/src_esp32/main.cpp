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

#include "board_functions.hpp"
#include "gpio_actor.hpp"
#include "lua.hpp"
#include "lua_runtime.hpp"
#include "managed_actor.hpp"
#include "native_runtime.hpp"
#include "pubsub/router.hpp"
#include "tcp_forwarder.hpp"
#include "wifi_stack.hpp"
extern "C" {
void app_main(void);
}

#if CONFIG_LEGACY_BENCHMARK_ENABLED

#include "../legacy/benchmark_actor.hpp"
#include "../legacy/benchmark_configuration.hpp"
#include "../legacy/benchmark_lua_actor.hpp"
#include "../legacy/include/message.hpp"
#include "../legacy/include/router_v2.hpp"
// #include "../legacy/router.hpp"

struct BenchmarkParams {
  uint64_t id;
  RouterV2* router;
};

void actor_task(void* params) {
  RouterV2* router = ((struct BenchmarkParams*)params)->router;
  uint64_t id = ((struct BenchmarkParams*)params)->id;

#if LUA_ACTORS
#if LUA_SHARED_RUNTIME
  lua_State* lua_state = LuaActor::create_state();
#else
  lua_State* lua_state = nullptr;
#endif
  LuaActor* actor[ACTORS_PER_THREAD];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    actor[i] = new LuaActor(id * ACTORS_PER_THREAD + i, lua_state);
  }
#else
  Actor* actor[ACTORS_PER_THREAD];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    actor[i] = new Actor(id * ACTORS_PER_THREAD + i);
  }
#endif
  char primary[128], buffer[128];
  snprintf(primary, sizeof(primary), "%lld", id * ACTORS_PER_THREAD);
  router->register_actor(primary);
  for (int i = 1; i < ACTORS_PER_THREAD; i++) {
    snprintf(buffer, sizeof(buffer), "%lld", id * ACTORS_PER_THREAD + i);
    router->register_alias(primary, buffer);
  }

  while (true) {
    auto message = router->receive(primary, portMAX_DELAY);
    if (message) {
      actor[std::stoi(message->receiver()) - id * ACTORS_PER_THREAD]->receive(
          std::stoi(message->sender()), const_cast<char*>(message->buffer()));
    }
  }
}

void real_main(void*) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());
  RouterV2* router = &RouterV2::getInstance();

  // TaskHandle_t handles[NUM_THREADS] = {NULL};
  BenchmarkParams* params = static_cast<BenchmarkParams*>(
      pvPortMalloc(sizeof(BenchmarkParams) * NUM_THREADS));

  for (uint64_t i = 0; i < NUM_THREADS; i++) {
    params[i] = {.id = i, .router = router};
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "Test%lld", i);
    xTaskCreatePinnedToCore(&actor_task, buffer, 7500, &params[i], 5, nullptr,
                            i % 2);
  }

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  router->send(Message("0", "1", 1234, ""));
  vTaskDelete(nullptr);
}

void app_main(void) {
  xTaskCreatePinnedToCore(&real_main, "MAIN", 7500, nullptr, 5, nullptr, 0);
}
#else

#if CONFIG_BENCHMARK_LOCAL
void main_task_local(void*) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());

  Params params = {.node_id = BoardFunctions::NODE_ID, .instance_id = "1"};

  xTaskCreatePinnedToCore(&NativeRuntime::os_task, "BASIC_RUNTIME", 6168,
                          &params, 5, nullptr, 0);

  BoardFunctions::epoch = 0;

  vTaskDelay(4000 / portTICK_PERIOD_MS);

  auto create_deployment_manager =
      uActor::PubSub::Publication(BoardFunctions::NODE_ID, "root", "1");
  create_deployment_manager.set_attr("command", "spawn_native_actor");
  create_deployment_manager.set_attr("spawn_code", "");
  create_deployment_manager.set_attr("spawn_node_id", BoardFunctions::NODE_ID);
  create_deployment_manager.set_attr("spawn_actor_type", "deployment_manager");
  create_deployment_manager.set_attr("spawn_instance_id", "1");
  create_deployment_manager.set_attr("node_id", BoardFunctions::NODE_ID);
  create_deployment_manager.set_attr("actor_type", "native_runtime");
  create_deployment_manager.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(
      std::move(create_deployment_manager));

  vTaskDelay(1000 / portTICK_PERIOD_MS);

  {
    uActor::PubSub::Publication label_update(BoardFunctions::NODE_ID, "root",
                                             "1");
    label_update.set_attr("type", "label_update");
    label_update.set_attr("command", "upsert");
    label_update.set_attr("node_id", BoardFunctions::NODE_ID);
    label_update.set_attr("key", "node_id");
    label_update.set_attr("value", BoardFunctions::NODE_ID);
    uActor::PubSub::Router::get_instance().publish(std::move(label_update));
  }

  for (const auto label : BoardFunctions::node_labels()) {
    uActor::PubSub::Publication label_update(BoardFunctions::NODE_ID, "root",
                                             "1");
    label_update.set_attr("type", "label_update");
    label_update.set_attr("command", "upsert");
    label_update.set_attr("node_id", BoardFunctions::NODE_ID);
    label_update.set_attr("key", label.first);
    label_update.set_attr("value", label.second);
    uActor::PubSub::Router::get_instance().publish(std::move(label_update));
  }

  xTaskCreatePinnedToCore(&LuaRuntime::os_task, "LUA_RUNTIME", 8192, &params,
                          10, nullptr, 1);

  xTaskCreatePinnedToCore(&GPIOActor::os_task, "GPIO_ACTOR", 4192, nullptr, 5,
                          nullptr, 0);

  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());

  vTaskDelay(5000 / portTICK_PERIOD_MS);

  auto example_deployment = uActor::PubSub::Publication("foo", "root", "1");
  example_deployment.set_attr("type", "deployment");
  example_deployment.set_attr("deployment_name", "example");
  example_deployment.set_attr("deployment_actor_type", "foo");
  example_deployment.set_attr("deployment_actor_version", "0.1");
  example_deployment.set_attr("deployment_actor_runtime_type", "lua");
  example_deployment.set_attr("deployment_actor_code", R"(
-- code here
  )");
  example_deployment.set_attr("deployment_required_actors", "core.io.gpio");
  example_deployment.set_attr("deployment_ttl", 0);
  example_deployment.set_attr("_internal_sequence_number", 1);
  example_deployment.set_attr("_internal_epoch", 0);

  uActor::PubSub::Router::get_instance().publish(std::move(example_deployment));

  vTaskDelete(nullptr);
}
#endif

void main_task(void*) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());

  xTaskCreatePinnedToCore(&uActor::PubSub::Router::os_task, "Router", 4192,
                          nullptr, 3, nullptr, 0);

  Params params = {.node_id = BoardFunctions::NODE_ID, .instance_id = "1"};

  xTaskCreatePinnedToCore(&WifiStack::os_task, "WIFI_STACK", 4192, nullptr, 4,
                          nullptr, 0);
  xTaskCreatePinnedToCore(&NativeRuntime::os_task, "BASIC_RUNTIME", 6168,
                          &params, 5, nullptr, 0);

  time_t t = 0;
  while (t < 1577836800) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    time(&t);
  }

  if (t > 1577836800) {
    t -= 1577836800;
    BoardFunctions::epoch = t;
    printf("epoch %ld\n", t);
  } else {
    printf("Epoch not set\n");
  }

  auto create_deployment_manager =
      uActor::PubSub::Publication(BoardFunctions::NODE_ID, "root", "1");
  create_deployment_manager.set_attr("command", "spawn_native_actor");
  create_deployment_manager.set_attr("spawn_code", "");
  create_deployment_manager.set_attr("spawn_node_id", BoardFunctions::NODE_ID);
  create_deployment_manager.set_attr("spawn_actor_type", "deployment_manager");
  create_deployment_manager.set_attr("spawn_instance_id", "1");
  create_deployment_manager.set_attr("node_id", BoardFunctions::NODE_ID);
  create_deployment_manager.set_attr("actor_type", "native_runtime");
  create_deployment_manager.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(
      std::move(create_deployment_manager));

  auto create_topology_manager =
      uActor::PubSub::Publication(BoardFunctions::NODE_ID, "root", "1");
  create_topology_manager.set_attr("command", "spawn_native_actor");
  create_topology_manager.set_attr("spawn_code", "");
  create_topology_manager.set_attr("spawn_node_id", BoardFunctions::NODE_ID);
  create_topology_manager.set_attr("spawn_actor_type", "topology_manager");
  create_topology_manager.set_attr("spawn_instance_id", "1");
  create_topology_manager.set_attr("node_id", BoardFunctions::NODE_ID);
  create_topology_manager.set_attr("actor_type", "native_runtime");
  create_topology_manager.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(
      std::move(create_topology_manager));

  if (std::string("node_home") == BoardFunctions::NODE_ID) {
    auto create_bmp180_sensor =
        uActor::PubSub::Publication(BoardFunctions::NODE_ID, "root", "1");
    create_bmp180_sensor.set_attr("command", "spawn_native_actor");
    create_bmp180_sensor.set_attr("spawn_code", "");
    create_bmp180_sensor.set_attr("spawn_node_id", BoardFunctions::NODE_ID);
    create_bmp180_sensor.set_attr("spawn_actor_type", "bmp180_sensor");
    create_bmp180_sensor.set_attr("spawn_instance_id", "1");
    create_bmp180_sensor.set_attr("node_id", BoardFunctions::NODE_ID);
    create_bmp180_sensor.set_attr("actor_type", "native_runtime");
    create_bmp180_sensor.set_attr("instance_id", "1");
    uActor::PubSub::Router::get_instance().publish(
        std::move(create_bmp180_sensor));
  }

  vTaskDelay(50 / portTICK_PERIOD_MS);
  {
    uActor::PubSub::Publication label_update(BoardFunctions::NODE_ID, "root",
                                             "1");
    label_update.set_attr("type", "label_update");
    label_update.set_attr("command", "upsert");
    label_update.set_attr("node_id", BoardFunctions::NODE_ID);
    label_update.set_attr("key", "core.node_id");
    label_update.set_attr("value", BoardFunctions::NODE_ID);
    uActor::PubSub::Router::get_instance().publish(std::move(label_update));
  }
  for (const auto label : BoardFunctions::node_labels()) {
    uActor::PubSub::Publication label_update(BoardFunctions::NODE_ID, "root",
                                             "1");
    label_update.set_attr("type", "label_update");
    label_update.set_attr("command", "upsert");
    label_update.set_attr("node_id", BoardFunctions::NODE_ID);
    label_update.set_attr("key", label.first);
    label_update.set_attr("value", label.second);
    uActor::PubSub::Router::get_instance().publish(std::move(label_update));
  }

  xTaskCreatePinnedToCore(&LuaRuntime::os_task, "LUA_RUNTIME", 8192, &params,
                          configMAX_PRIORITIES - 1, nullptr, 1);

  xTaskCreatePinnedToCore(&GPIOActor::os_task, "GPIO_ACTOR", 4192, nullptr, 5,
                          nullptr, 0);

  xTaskCreatePinnedToCore(&TCPForwarder::os_task, "TCP", 4192, nullptr, 4,
                          nullptr, 0);

  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());

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

#endif
