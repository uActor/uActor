#include <unistd.h>

#include <utility>
#include <actor_runtime/executor.hpp>
#include <actor_runtime/lua_executor.hpp>
#include <actor_runtime/native_executor.hpp>
#include <remote/tcp_forwarder.hpp>
#include <thread>
#include <iostream>

std::thread start_lua_executor() {
  uActor::ActorRuntime::ExecutorSettings* params = new uActor::ActorRuntime::ExecutorSettings{.node_id = "node_linux", .instance_id = "1"};
  std::thread executor_thread = std::thread(&uActor::ActorRuntime::LuaExecutor::os_task, params);
  return std::move(executor_thread);
}

std::thread start_native_executor() {
  uActor::ActorRuntime::ExecutorSettings* params = new uActor::ActorRuntime::ExecutorSettings{.node_id = "node_linux", .instance_id = "1"};
  std::thread executor_thread = std::thread(&uActor::ActorRuntime::NativeExecutor::os_task, params);
  return std::move(executor_thread);
}

int main() {
  std::cout << "starting" << std::endl;
  uActor::BoardFunctions::epoch = 0;
  auto router_task =   std::thread(&uActor::PubSub::Router::os_task, nullptr);
  auto tcp_task =  std::thread(&uActor::Linux::Remote::TCPForwarder::os_task, nullptr);
  auto native_executor = start_native_executor();

  sleep(2);
  
  auto create_deployment_manager =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
  create_deployment_manager.set_attr("command", "spawn_native_actor");
  create_deployment_manager.set_attr("spawn_code", "");
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
  create_topology_manager.set_attr("spawn_code", "");
  create_topology_manager.set_attr("spawn_node_id",
                                   uActor::BoardFunctions::NODE_ID);
  create_topology_manager.set_attr("spawn_actor_type", "topology_manager");
  create_topology_manager.set_attr("spawn_instance_id", "1");
  create_topology_manager.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
  create_topology_manager.set_attr("actor_type", "native_executor");
  create_topology_manager.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(
      std::move(create_topology_manager));

  sleep(2);

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


  auto lua_executor = start_lua_executor();

  tcp_task.join();
  return 0;
}