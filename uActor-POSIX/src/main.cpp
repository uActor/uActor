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
#include "actor_runtime/lua_executor.hpp"
#include "actor_runtime/managed_actor.hpp"
#include "actor_runtime/managed_native_actor.hpp"
#include "actor_runtime/native_executor.hpp"
#if CONFIG_UACTOR_V8
#include "actor_runtime/v8/executor.hpp"
#endif
#include "remote/remote_connection.hpp"
#include "remote/tcp_forwarder.hpp"
#include "support/core_native_actors.hpp"
#include "support/launch_utils.hpp"
#include "support/posix_native_actors.hpp"

#if CONFIG_UACTOR_ENABLE_TELEMETRY
#include "controllers/telemetry_actor.hpp"
#include "controllers/telemetry_data.hpp"
#endif

#if CONFIG_BENCHMARK_ENABLED
#include "support/testbed.h"
#endif

#if CONFIG_UACTOR_ENABLE_HTTP_CLIENT_ACTOR
#include "actors/http_client_actor.hpp"
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
      "total_queue_size", uActor::PubSub::Receiver::total_queue_size.load());

  uActor::Controllers::TelemetryData::set(
      "total_actor_queue_size",
      uActor::ActorRuntime::ManagedActor::total_queue_size.load());

  uActor::Controllers::TelemetryData::set(
      "number_of_subscriptions",
      uActor::PubSub::Router::get_instance().number_of_subscriptions());
}
#endif

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
    "cluster-labels",
    boost::program_options::value<std::string>(),
    "Comma-separated list of labels that are used"
    "for cluster aggregation and subscription containment."
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
  )
  (
    "tcp-static-peer", boost::program_options::value<std::string>(),
    "automatically connect to a TCP peer."
  )
#if CONFIG_UACTOR_ENABLE_INFLUXDB_ACTOR
  (
    "influxdb-url", boost::program_options::value<std::string>(),
    "InfluxDB host"
  )
#endif
  (
    "enable-code-server", boost::program_options::bool_switch(),
    "Add subscription that lets the node respond to remote code fetch requests."
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
  auto arguments = parse_arguments(arg_count, args);

  if (arguments.count("node-id") != 0u) {
    uActor::BoardFunctions::NODE_ID =
        (new std::string(arguments["node-id"].as<std::string>()))->data();
  }

  uActor::Support::Logger::info("MAIN", "Starting uActor. Node ID: %s",
                                uActor::BoardFunctions::NODE_ID);

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
    uActor::Support::Logger::warning("MAIN",
                                     "Timestamp lower than the magic offset!");
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
    uActor::BoardFunctions::sleep(100);
  }

  auto tcp_task2 = std::thread(&uActor::Remote::TCPForwarder::tcp_reader_task,
                               tcp_task_args.tcp_forwarder);

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  uActor::Controllers::TelemetryActor::telemetry_fetch_hook =
      telemetry_fetch_hook;
#endif

  uActor::Support::CoreNativeActors::register_native_actors();
  uActor::Support::PosixNativeActors::register_native_actors();

  auto native_executor =
      uActor::Support::LaunchUtils::await_start_native_executor<std::thread>(
          [](uActor::ActorRuntime::ExecutorSettings* params) {
            return std::thread(&uActor::ActorRuntime::NativeExecutor::os_task,
                               params);
          });

  uActor::Support::LaunchUtils::await_spawn_native_actor("deployment_manager");
  uActor::Support::LaunchUtils::await_spawn_native_actor("topology_manager");
  uActor::Support::LaunchUtils::await_spawn_native_actor("code_store");

  auto lua_executor =
      uActor::Support::LaunchUtils::await_start_lua_executor<std::thread>(
          [](uActor::ActorRuntime::ExecutorSettings* params) {
            return std::thread(&uActor::ActorRuntime::LuaExecutor::os_task,
                               params);
          });
#if CONFIG_UACTOR_V8
  auto v8_executor =
      uActor::Support::LaunchUtils::await_start_v8_executor<std::thread>(
          [](uActor::ActorRuntime::ExecutorSettings* params) {
            return std::thread(
                &uActor::ActorRuntime::V8Runtime::V8Executor::os_task, params);
          });
#endif

#if CONFIG_UACTOR_ENABLE_INFLUXDB_ACTOR
  if (arguments.count("influxdb-url")) {
    uActor::Database::InfluxDBActor::server_url =
        arguments["influxdb-url"].as<std::string>();
    uActor::Support::LaunchUtils::await_spawn_native_actor(
        "influxdb_connector");
  }
#endif

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  uActor::Support::LaunchUtils::await_spawn_native_actor("telemetry_actor");
#endif

#if CONFIG_UACTOR_ENABLE_HTTP_CLIENT_ACTOR
  uActor::HTTP::HTTPClientActor http_client{};
#endif

#if CONFIG_UACTOR_ENABLE_HTTP_INGRESS
  uActor::Support::LaunchUtils::await_spawn_native_actor("http_ingress");
#endif

  if (arguments["enable-code-server"].as<bool>()) {
    auto enable_message = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "1");
    enable_message.set_attr("type", "remote_fetch_control_command");
    enable_message.set_attr("command", "enable");
    uActor::PubSub::Router::get_instance().publish(std::move(enable_message));
  }

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

  if (arguments.count("cluster-labels")) {
    auto raw = arguments["cluster-labels"].as<std::string>();
    auto raw_split = uActor::Support::StringHelper::string_split(raw);
    uActor::Remote::RemoteConnection::clusters.emplace_back(raw_split.begin(),
                                                            raw_split.end());
  }

  if (arguments.count("node-label")) {
    auto raw_labels = arguments["node-label"].as<std::vector<std::string>>();

    std::list<std::pair<std::string, std::string>> labels;
    for (const auto& raw_label : raw_labels) {
      uint32_t split_pos = raw_label.find_first_of(":");

      std::string key = raw_label.substr(0, split_pos);
      std::string value = raw_label.substr(split_pos + 1);

      for (const auto& cluster : uActor::Remote::RemoteConnection::clusters) {
        if (std::find(cluster.begin(), cluster.end(), key) != cluster.end()) {
          uActor::Remote::RemoteConnection::local_location_labels[key] = value;
        }
      }

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

  if (arguments.count("tcp-static-peer")) {
    auto raw_static_peers = arguments["tcp-static-peer"].as<std::string>();
    for (std::string_view raw_static_peer :
         uActor::Support::StringHelper::string_split(raw_static_peers)) {
      uint32_t first_split_pos = raw_static_peer.find_first_of(":");
      uint32_t second_split_pos = raw_static_peer.find_last_of(":");

      std::string_view peer_node_id =
          raw_static_peer.substr(0, first_split_pos);
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
  }

#if CONFIG_BENCHMARK_ENABLED
  sleep(1);
  testbed_log_rt_string("node_id", uActor::BoardFunctions::NODE_ID);
  testbed_log_rt_integer("_ready", 1);
#endif

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
