#include <unistd.h>

#include <utility>
#include <actor_runtime/executor.hpp>
#include <actor_runtime/lua_executor.hpp>
#include <actor_runtime/native_executor.hpp>
#include <remote/tcp_forwarder.hpp>
#include <thread>
#include <iostream>
#include <vector>

#include <boost/program_options.hpp>

std::thread start_lua_executor() {
  uActor::ActorRuntime::ExecutorSettings* params = new uActor::ActorRuntime::ExecutorSettings{.node_id = uActor::BoardFunctions::NODE_ID, .instance_id = "1"};
  std::thread executor_thread = std::thread(&uActor::ActorRuntime::LuaExecutor::os_task, params);
  return std::move(executor_thread);
}

std::thread start_native_executor() {
  uActor::ActorRuntime::ExecutorSettings* params = new uActor::ActorRuntime::ExecutorSettings{.node_id = uActor::BoardFunctions::NODE_ID, .instance_id = "1"};
  std::thread executor_thread = std::thread(&uActor::ActorRuntime::NativeExecutor::os_task, params);
  return std::move(executor_thread);
}

boost::program_options::variables_map parse_arguments(int arg_count, char** args) {
  boost::program_options::options_description desc("Options");
  desc.add_options()
      ("help", "produce help message")
      ("node-id", boost::program_options::value<std::string>(), "set node id")
      ("server-node", boost::program_options::value<std::string>(), "peer to connect to")
      ("node-labels", boost::program_options::value<std::string>(), "node labels (comma seperated)")
      ("tcp-port", boost::program_options::value<uint>(), "tcp port");

  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::parse_command_line(arg_count, args, desc), arguments);
  boost::program_options::notify(arguments);

  if (arguments.count("help")) {
    std::cout << desc << "\n";
    exit(1);
  } else {
    return std::move(arguments);
  }
};

int main(int arg_count, char** args) {
  std::cout << "starting uActor" << std::endl;

  auto arguments = parse_arguments(arg_count, args);

  if (arguments.count("node-id")) {
    uActor::BoardFunctions::NODE_ID = (new std::string(arguments["node-id"].as<std::string>()))->data();
  }

  if(arguments.count("server-node")) {
    uActor::BoardFunctions::SERVER_NODES = std::vector<std::string>{arguments["server-node"].as<std::string>()};
  }
  
  uActor::BoardFunctions::epoch = 0;

  auto router_task =   std::thread(&uActor::PubSub::Router::os_task, nullptr);
  
  int tcp_port = 1337;
  if(arguments.count("tcp-port")) {
    tcp_port = arguments["tcp-port"].as<uint>();
  }

  auto tcp_task =  std::thread(&uActor::Linux::Remote::TCPForwarder::os_task, reinterpret_cast<void*>(&tcp_port));
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
    uActor::PubSub::Publication label_update(uActor::BoardFunctions::NODE_ID, "root", "1");
    label_update.set_attr("type", "label_update");
    label_update.set_attr("command", "upsert");
    label_update.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    label_update.set_attr("key", "node_id");
    label_update.set_attr("value", uActor::BoardFunctions::NODE_ID);
    uActor::PubSub::Router::get_instance().publish(std::move(label_update));
  }

  if(arguments.count("node-labels")) {
    auto raw_labels = arguments["node-labels"].as<std::string>();

    std::list<std::pair<std::string, std::string>> labels;
    for (std::string_view raw_label : uActor::Support::StringHelper::string_split(raw_labels)) {
      uint32_t split_pos = raw_label.find_first_of("=");
      
      std::string_view key = raw_label.substr(0, split_pos);
      std::string_view value = raw_label.substr(split_pos + 1);

      uActor::PubSub::Publication label_update(uActor::BoardFunctions::NODE_ID, "root", "1");
      label_update.set_attr("type", "label_update");
      label_update.set_attr("command", "upsert");
      label_update.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
      label_update.set_attr("key", key);
      label_update.set_attr("value", value);
      uActor::PubSub::Router::get_instance().publish(std::move(label_update));
    }
  }


  auto lua_executor = start_lua_executor();

  tcp_task.join();
  return 0;
}