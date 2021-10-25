#pragma once

#include <pubsub/router.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace uActor::Support {
struct LaunchUtils {
  static void await_spawn_native_actor(
      std::string actor_type, std::string native_executor_instance_id = "1") {
    auto r = uActor::PubSub::Router::get_instance().new_subscriber();

    r.subscribe(
        uActor::PubSub::Filter{
            uActor::PubSub::Constraint{"type", "actor_creation"},
            uActor::PubSub::Constraint{"category", "actor_lifetime"},
            uActor::PubSub::Constraint{"lifetime_actor_type", actor_type},
            uActor::PubSub::Constraint{"lifetime_instance_id", "0"},
            uActor::PubSub::Constraint{"publisher_node_id",
                                       uActor::BoardFunctions::NODE_ID}},
        PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "start_helper", "1"));

    auto spawn_message = uActor::PubSub::Publication(
        uActor::BoardFunctions::NODE_ID, "root", "0");
    spawn_message.set_attr("command", "spawn_native_actor");
    spawn_message.set_attr("spawn_actor_version", "default");
    spawn_message.set_attr("spawn_node_id", uActor::BoardFunctions::NODE_ID);
    spawn_message.set_attr("spawn_actor_type", actor_type);
    spawn_message.set_attr("spawn_instance_id", "0");
    spawn_message.set_attr("node_id", uActor::BoardFunctions::NODE_ID);
    spawn_message.set_attr("actor_type", "native_executor");
    spawn_message.set_attr("instance_id", native_executor_instance_id);
    uActor::PubSub::Router::get_instance().publish(std::move(spawn_message));

    auto res = r.receive(uActor::BoardFunctions::SLEEP_FOREVER);
    assert(res);
  }

  template <typename T>
  static T await_start_lua_executor(
      std::function<T(ActorRuntime::ExecutorSettings*)> launch) {
    auto r = uActor::PubSub::Router::get_instance().new_subscriber();
    r.subscribe(
        uActor::PubSub::Filter{
            uActor::PubSub::Constraint{"type", "executor_update"},
            uActor::PubSub::Constraint{"command", "register"},
            uActor::PubSub::Constraint{"update_actor_type", "lua_executor"},
            uActor::PubSub::Constraint{"update_instance_id", "1"},
            uActor::PubSub::Constraint{"publisher_node_id",
                                       uActor::BoardFunctions::NODE_ID}},
        PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "start_helper", "1"));
    uActor::ActorRuntime::ExecutorSettings* params =
        new uActor::ActorRuntime::ExecutorSettings{
            .node_id = uActor::BoardFunctions::NODE_ID, .instance_id = "1"};
    T spawn_result = launch(params);
    auto res = r.receive(uActor::BoardFunctions::SLEEP_FOREVER);
    assert(res);
    return spawn_result;
  }

  template <typename T>
  static T await_start_native_executor(
      std::function<T(ActorRuntime::ExecutorSettings*)> launch,
      std::string instance_id = "1") {
    auto r = uActor::PubSub::Router::get_instance().new_subscriber();
    r.subscribe(
        uActor::PubSub::Filter{
            uActor::PubSub::Constraint{"type", "executor_update"},
            uActor::PubSub::Constraint{"command", "register"},
            uActor::PubSub::Constraint{"update_actor_type", "native_executor"},
            uActor::PubSub::Constraint{"update_instance_id", instance_id},
            uActor::PubSub::Constraint{"publisher_node_id",
                                       uActor::BoardFunctions::NODE_ID}},
        PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "start_helper", "1"));
    uActor::ActorRuntime::ExecutorSettings* params =
        new uActor::ActorRuntime::ExecutorSettings{
            .node_id = uActor::BoardFunctions::NODE_ID,
            .instance_id = instance_id.c_str()};
    T spawn_result = launch(params);
    auto res = r.receive(uActor::BoardFunctions::SLEEP_FOREVER);
    assert(res);
    return spawn_result;
  }
};
}  // namespace uActor::Support
