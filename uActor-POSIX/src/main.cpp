#include <unistd.h>

#include <boost/program_options.hpp>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include "actor_runtime/code_store_actor.hpp"
#include "actor_runtime/executor.hpp"
#include "actor_runtime/lua_executor.hpp"
#include "actor_runtime/managed_native_actor.hpp"
#include "actor_runtime/native_executor.hpp"
#include "actors/influxdb_actor.hpp"
#include "controllers/deployment_manager.hpp"
#include "controllers/topology_manager.hpp"
#include "remote/remote_connection.hpp"
#include "remote/tcp_forwarder.hpp"

#if CONFIG_UACTOR_ENABLE_TELEMETRY
#include "controllers/telemetry_actor.hpp"
#include "controllers/telemetry_data.hpp"
#endif

#if CONFIG_BENCHMARK_ENABLED
#include "support/testbed.h"
#endif

/*
 *  Offset to be subtracted from the current timestamp to
 *  set the epoch number
 */
#define MAGIC_EPOCH_OFFSET 1590969600

#if CONFIG_UACTOR_ENABLE_TELEMETRY
void telemetry_fetch_hook() {
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
      "current_queue_size_diff",
      uActor::PubSub::Receiver::size_diff.exchange(0));

  uActor::Controllers::TelemetryData::set(
      "number_of_subscriptions",
      uActor::PubSub::Router::get_instance().number_of_subscriptions());
}
#endif

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
  #if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
  )
  (
    "telemetry-file", boost::program_options::value<std::string>(),
    "Log seconds-accuracy telemetry to this file."
  #endif
  ); // NOLINT
  // clang-format on

  boost::program_options::variables_map arguments;
  boost::program_options::store(
      boost::program_options::parse_command_line(arg_count, args, desc),
      arguments);
  boost::program_options::notify(arguments);

  if (arguments.count("help") != 0u) {
    std::cout << desc << "\n";
    exit(1);
  } else {
    return std::move(arguments);
  }
}

#if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
std::ofstream telemetry_file;
#endif

int main(int arg_count, char** args) {
  std::cout << "starting uActor" << std::endl;

  auto arguments = parse_arguments(arg_count, args);

  if (arguments.count("node-id") != 0u) {
    uActor::BoardFunctions::NODE_ID =
        (new std::string(arguments["node-id"].as<std::string>()))->data();
  }

  if (arguments.count("server-node") != 0u) {
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

  int tcp_port = 1337;
  if (arguments.count("tcp-port") != 0u) {
    tcp_port = arguments["tcp-port"].as<uint16_t>();
  }

  std::string listen_ip = "0.0.0.0";
  if (arguments.count("tcp-listen-ip") != 0u) {
    listen_ip = arguments["tcp-listen-ip"].as<std::string>();
  }

  std::string external_address{};
  if (arguments.count("tcp-external-address") != 0u) {
    external_address = arguments["tcp-external-address"].as<std::string>();
  }

  uint16_t external_port = 0;
  if (arguments.count("tcp-external-port") != 0u) {
    external_port = arguments["tcp-external-port"].as<uint16_t>();
  }

  auto tcp_task_args = uActor::Remote::TCPAddressArguments(
      listen_ip, tcp_port, external_address, external_port);

  auto tcp_task = std::thread(&uActor::Remote::TCPForwarder::os_task,
                              reinterpret_cast<void*>(&tcp_task_args));

  while (tcp_task_args.tcp_forwarder == nullptr) {
    sleep(1);
  }

  auto tcp_task2 = std::thread(&uActor::Remote::TCPForwarder::tcp_reader_task,
                               tcp_task_args.tcp_forwarder);

  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Controllers::TopologyManager>("topology_manager");
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Controllers::DeploymentManager>("deployment_manager");
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ActorRuntime::CodeStoreActor>("code_store");
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Database::InfluxDBActor>("influxdb_connector");
  auto nativeexecutor = start_native_executor();
#if CONFIG_UACTOR_ENABLE_TELEMETRY
  uActor::Controllers::TelemetryActor::telemetry_fetch_hook =
      telemetry_fetch_hook;
  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::Controllers::TelemetryActor>("telemetry_actor");
#endif

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

  auto create_influxdb_actor =
      uActor::PubSub::Publication(uActor::BoardFunctions::NODE_ID, "root", "1");
  create_influxdb_actor.set_attr("command", "spawn_native_actor");
  create_influxdb_actor.set_attr("spawn_actor_version", "default");
  create_influxdb_actor.set_attr("spawn_node_id",
                                 uActor::BoardFunctions::NODE_ID);
  create_influxdb_actor.set_attr("spawn_actor_type", "influxdb_connector");
  create_influxdb_actor.set_attr("spawn_instance_id", "1");
  create_influxdb_actor.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
  create_influxdb_actor.set_attr("actor_type", "native_executor");
  create_influxdb_actor.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(
      std::move(create_influxdb_actor));

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

#if CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY
  if (arguments.count("telemetry-file")) {
    sleep(2);
    testbed_log_rt_integer("_ready", boot_timestamp);

    struct sigaction flush_handler;
    flush_handler.sa_handler = [](int signum) {
      telemetry_file.flush();
      telemetry_file.close();
      exit(signum);
    };
    sigemptyset(&flush_handler.sa_mask);
    flush_handler.sa_flags = 0;
    sigaction(SIGINT, &flush_handler, 0);

    auto path_string = arguments["telemetry-file"].as<std::string>();
    auto path = std::filesystem::path(path_string);
    std::filesystem::create_directories(path.parent_path());

    telemetry_file.open(path, std::ios::out);
    assert(telemetry_file.is_open());
    do {
      auto timestamp = uActor::BoardFunctions::seconds_timestamp();

      telemetry_fetch_hook();
      auto last_second_telemetry =
          uActor::Controllers::TelemetryData::replace_seconds_instance();

      for (const auto& [key, value] : last_second_telemetry.data) {
        telemetry_file << timestamp << "," << key << "," << value << std::endl;
      }

      sleep(1);
    } while (true);
  }
#endif

  tcp_task2.join();
  tcp_task.join();
  return 0;
}
