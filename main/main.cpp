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

#include "benchmark_configuration.hpp"
#include "include/board_functions.hpp"
#include "include/lua_runtime.hpp"
#include "include/managed_actor.hpp"
#include "include/message.hpp"
#include "include/router_v2.hpp"
#include "lua.hpp"
#include "tcp_forwarder.hpp"
#include "wifi_stack.hpp"

extern "C" {
void app_main(void);
}

#if BENCHMARK

#include "benchmark_actor.hpp"
#include "benchmark_lua_actor.hpp"
// #include "router.hpp"

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

const char receive_fun[] = R"(
function receive(message)
  print(message.sender_node_id.."."..message.sender_actor_type.."."..message.sender_instance_id.." -> "..node_id.."."..actor_type.."."..instance_id);
  if(instance_id == "1") then
    if(message.type == "init") then
      sub_id = subscribe({node_id=node_id, instance_id="foo", actor_type=actor_type})
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="delayed_publish"}, 2000);
    elseif(message.type == "delayed_publish") then
      send({node_id=node_id, actor_type=actor_type, message="ping"});
    end
  elseif(instance_id=="2" and (not (message.type == "init"))) then
    send({node_id=node_id, instance_id="foo", actor_type=actor_type, message="pong"})
    deferred_block_for({foo="bar"}, 20000);
  elseif(not (message.type == "init")) then
    send({node_id=message.sender_node_id, instance_id=message.sender_instance_id, actor_type=message.sender_actor_type, message="pong"})
  end
end
)";

void main_task(void*) {
  printf("InitialHeap: %d \n", xPortGetFreeHeapSize());
  TaskHandle_t handle = {NULL};

  Params params = {.node_id = "node_1", .instance_id = "1"};

  xTaskCreatePinnedToCore(&LuaRuntime::os_task, "TEST", 6168, &params, 4,
                          &handle, 1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  char name[128];
  for (int i = 0; i < ACTORS_PER_THREAD; i++) {
    snprintf(name, sizeof(name), "%d", i + 1);
    Publication create_actor = Publication("node_1", "root", "1");
    create_actor.set_attr("command", "spawn_lua_actor");
    create_actor.set_attr("spawn_code", receive_fun);
    create_actor.set_attr("spawn_node_id", "node_1");
    create_actor.set_attr("spawn_actor_type", "actor");
    create_actor.set_attr("spawn_instance_id", name);
    create_actor.set_attr("node_id", "node_1");
    create_actor.set_attr("actor_type", "lua_runtime");
    create_actor.set_attr("instance_id", "1");
    PubSub::Router::get_instance().publish(std::move(create_actor));
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  printf("StaticHeap: %d \n", xPortGetFreeHeapSize());
  while (true) {
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Heap: %d \n", xPortGetFreeHeapSize());
  }
}

void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  xTaskCreatePinnedToCore(&main_task, "MAIN", 6168, nullptr, 5, nullptr, 1);
  xTaskCreatePinnedToCore(&WifiStack::os_task, "FORWARDER", 6168, nullptr, 4,
                          nullptr, 1);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  xTaskCreatePinnedToCore(&TCPForwarder::os_task, "TCP", 6168, nullptr, 4,
                          nullptr, 1);
}

#endif
