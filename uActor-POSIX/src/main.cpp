#include <unistd.h>

#include <boost/program_options.hpp>
#include <ctime>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include "actor_runtime/code_store.hpp"
#include "actor_runtime/executor.hpp"
#include "actor_runtime/lua_executor.hpp"
#include "actor_runtime/managed_native_actor.hpp"
#include "actor_runtime/native_executor.hpp"
#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"
#include "remote/remote_connection.hpp"
#include "remote/tcp_forwarder.hpp"
#if CONFIG_BENCHMARK_ENABLED
#include "support/testbed.h"
#endif

/*
 *  Offset to be subtracted from the current timestamp to
 *  set the epoch number
 */
#define MAGIC_EPOCH_OFFSET 1590969600

std::thread start_lua_executor() {
  uActor::ActorRuntime::ExecutorSettings* params =
      new uActor::ActorRuntime::ExecutorSettings{
          .node_id = uActor::BoardFunctions::NODE_ID, .instance_id = "1"};
  std::thread executor_thread =
      std::thread(&uActor::ActorRuntime::LuaExecutor::os_task, params);
  return std::move(executor_thread);
}

std::thread start_native_executor() {
  uActor::ActorRuntime::ExecutorSettings* params =
      new uActor::ActorRuntime::ExecutorSettings{
          .node_id = uActor::BoardFunctions::NODE_ID, .instance_id = "1"};
  std::thread executor_thread =
      std::thread(&uActor::ActorRuntime::NativeExecutor::os_task, params);
  return std::move(executor_thread);
}

boost::program_options::variables_map parse_arguments(int arg_count,
                                                      char** args) {
  boost::program_options::options_description desc("Options");
  // clang-format off
  desc.add_options()
  ("help", "produce this help message.")
  ("node-id", boost::program_options::value<std::string>(), "Set the node id.")
  (
    "server-node",
    boost::program_options::value<std::vector<std::string>>()
      -> multitoken() -> composing(),
   "List nodes to connect to if there is a path (can be specified many times)."
  )
  (
    "node-label",
    boost::program_options::value<std::vector<std::string>>()
      -> multitoken() -> composing(),
    "Add a node label ( key:value, can be specified many times)."
  )
  (
    "tcp-port",
    boost::program_options::value<uint16_t>(),
    "Set the port the TCP server will listen on."
  )
  (
    "tcp-listen-ip", boost::program_options::value<std::string>(),
    "Set the IP the TCP server will listen on."
  )
  (
    "tcp-external-address", boost::program_options::value<std::string>(),
    "Provide a hint on the external address this node can be reached at."
  )
  (
    "tcp-external-port", boost::program_options::value<uint16_t>(),
    "Provide a hint on the external port this node can be reached at."
  ); // NOLINT
  // clang-format on

  boost::program_options::variables_map arguments;
  boost::program_options::store(
      boost::program_options::parse_command_line(arg_count, args, desc),
      arguments);
  boost::program_options::notify(arguments);

  if (arguments.count("help")) {
    std::cout << desc << "\n";
    exit(1);
  } else {
    return std::move(arguments);
  }
}

int main(int arg_count, char** args) {
  std::cout << "starting uActor" << std::endl;

  auto arguments = parse_arguments(arg_count, args);

  if (arguments.count("node-id")) {
    uActor::BoardFunctions::NODE_ID =
        (new std::string(arguments["node-id"].as<std::string>()))->data();
  }

  if (arguments.count("server-node")) {
    uActor::BoardFunctions::SERVER_NODES =
        arguments["server-node"].as<std::vector<std::string>>();
  }

  time_t boot_timestamp = 0;
  time(&boot_timestamp);
  if (boot_timestamp >= MAGIC_EPOCH_OFFSET) {
    uActor::BoardFunctions::epoch = boot_timestamp - MAGIC_EPOCH_OFFSET;
  } else {
    /*
     * The timestamp is not set correctly.
     * If this only happens once, this will cause problems.
     */
    std::cout << "WARNING: Timestamp lower than the magic offset!" << std::endl;
    uActor::BoardFunctions::epoch = boot_timestamp;
  }

  auto router_task = std::thread(&uActor::PubSub::Router::os_task, nullptr);

  int tcp_port = 1337;
  if (arguments.count("tcp-port")) {
    tcp_port = arguments["tcp-port"].as<uint16_t>();
  }

  std::string listen_ip = "0.0.0.0";
  if (arguments.count("tcp-listen-ip")) {
    listen_ip = arguments["tcp-listen-ip"].as<std::string>();
  }

  std::string external_address = "";
  if (arguments.count("tcp-external-address")) {
    external_address = arguments["tcp-external-address"].as<std::string>();
  }

  uint16_t external_port = 0;
  if (arguments.count("tcp-external-port")) {
    external_port = arguments["tcp-external-port"].as<uint16_t>();
  }

  auto tcp_task_args = uActor::Remote::TCPAddressArguments(
      listen_ip, tcp_port, external_address, external_port);

  auto tcp_task = std::thread(&uActor::Remote::TCPForwarder::os_task,
                              reinterpret_cast<void*>(&tcp_task_args));

  while (!tcp_task_args.tcp_forwarder) {
    sleep(1);
  }

  auto tcp_task2 = std::thread(&uActor::Remote::TCPForwarder::tcp_reader_task,
                               tcp_task_args.tcp_forwarder);

  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Controllers::TopologyManager>("topology_manager");
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Controllers::DeploymentManager>("deployment_manager");
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ActorRuntime::CodeStore>("code_store");
  auto native_executor = start_native_executor();

  sleep(2);

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

  if (arguments.count("node-label")) {
    auto raw_labels = arguments["node-label"].as<std::vector<std::string>>();

    std::list<std::pair<std::string, std::string>> labels;
    for (const auto& raw_label : raw_labels) {
      uint32_t split_pos = raw_label.find_first_of(":");

      std::string key = raw_label.substr(0, split_pos);
      std::string value = raw_label.substr(split_pos + 1);

      uActor::PubSub::Publication label_update(uActor::BoardFunctions::NODE_ID,
                                               "root", "1");
      label_update.set_attr("type", "label_update");
      label_update.set_attr("command", "upsert");
      label_update.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
      label_update.set_attr("key", key);
      label_update.set_attr("value", value);
      uActor::PubSub::Router::get_instance().publish(std::move(label_update));
    }
  }

  auto lua_executor = start_lua_executor();

#if CONFIG_BENCHMARK_ENABLED
  sleep(2);
  testbed_log_rt_integer("_ready", boot_timestamp);

  do {
    testbed_log_integer("current_message_information_timestamp",
                        uActor::BoardFunctions::seconds_timestamp());
    testbed_log_integer("current_accepted_message_count",
                        uActor::Remote::RemoteConnection::current_traffic
                            .num_accepted_messages.exchange(0));
    testbed_log_integer("current_accepted_message_size",
                        uActor::Remote::RemoteConnection::current_traffic
                            .size_accepted_messages.exchange(0));
    testbed_log_integer("current_rejected_message_count",
                        uActor::Remote::RemoteConnection::current_traffic
                            .num_duplicate_messages.exchange(0));
    testbed_log_integer("current_rejected_message_size",
                        uActor::Remote::RemoteConnection::current_traffic
                            .size_duplicate_messages.exchange(0));
    testbed_log_integer("current_sub_message_size",
                        uActor::Remote::RemoteConnection::current_traffic
                            .sub_traffic_size.exchange(0));
    testbed_log_integer("current_deployment_message_size",
                        uActor::Remote::RemoteConnection::current_traffic
                            .deployment_traffic_size.exchange(0));
    testbed_log_integer("current_regular_message_size",
                        uActor::Remote::RemoteConnection::current_traffic
                            .regular_traffic_size.exchange(0));
    testbed_log_integer("current_queue_size_max",
                        uActor::PubSub::Receiver::size_max.exchange(0));
    sleep(1);
  } while (true);
#endif

  tcp_task2.join();
  tcp_task.join();
  return 0;
}
