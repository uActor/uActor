#include <gtest/gtest.h>
#include <unistd.h>

#include <thread>

#include "actor_runtime/code_store_actor.hpp"
#include "actor_runtime/lua_executor.hpp"
#include "actor_runtime/native_executor.hpp"
#include "pubsub/publication.hpp"
#include "pubsub/receiver_handle.hpp"
#include "pubsub/router.hpp"

namespace uActor::Test {

struct Executors {
  Executors(std::thread lua_executor, std::thread native_executor)
      : lua_executor(std::move(lua_executor)),
        native_executor(std::move(native_executor)) {}

  std::thread lua_executor;
  std::thread native_executor;
};

void spawn_actor(std::string_view code, std::string_view instance_id) {
  auto publish_code = PubSub::Publication("node_1", "root", "1");

  auto code_version = std::to_string(std::hash<std::string_view>()(code));

  publish_code.set_attr("type", "actor_code");
  publish_code.set_attr("actor_code_type", "actor");
  publish_code.set_attr("actor_code_version", code_version);
  publish_code.set_attr("actor_code_lifetime_end",
                        static_cast<int32_t>(UINT32_MAX));
  publish_code.set_attr("actor_code_runtime_type", "lua");
  publish_code.set_attr("actor_code", std::string(code));

  PubSub::Router::get_instance().publish(std::move(publish_code));

  auto create_actor = PubSub::Publication("node_1", "root", "1");
  create_actor.set_attr("spawn_code", code);
  create_actor.set_attr("spawn_node_id", "node_1");
  create_actor.set_attr("spawn_actor_type", "actor");
  create_actor.set_attr("spawn_actor_version", code_version);
  create_actor.set_attr("spawn_instance_id", instance_id);
  create_actor.set_attr("node_id", "node_1");
  create_actor.set_attr("actor_type", "lua_executor");
  create_actor.set_attr("instance_id", "1");
  PubSub::Router::get_instance().publish(std::move(create_actor));
  sleep(1);
}

void shutdown_executors(Executors* executors) {
  auto exit_lua = PubSub::Publication("node_1", "root", "1");
  exit_lua.set_attr("node_id", "node_1");
  exit_lua.set_attr("instance_id", "1");
  exit_lua.set_attr("actor_type", "lua_executor");
  PubSub::Router::get_instance().publish(std::move(exit_lua));
  auto exit_native = PubSub::Publication("node_1", "root", "1");
  exit_native.set_attr("node_id", "node_1");
  exit_native.set_attr("instance_id", "1");
  exit_native.set_attr("actor_type", "native_executor");
  PubSub::Router::get_instance().publish(std::move(exit_native));
  executors->lua_executor.join();
  executors->native_executor.join();
}

PubSub::ReceiverHandle subscription_handle_with_default_subscription() {
  auto root_handle = PubSub::Router::get_instance().new_subscriber();
  PubSub::Filter primary_filter{
      PubSub::Constraint(std::string("node_id"), "node_1"),
      PubSub::Constraint(std::string("actor_type"), "root"),
      PubSub::Constraint(std::string("instance_id"), "1")};
  uint32_t root_subscription_id = root_handle.subscribe(primary_filter, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  return std::move(root_handle);
}

Executors start_executor_threads() {
  BoardFunctions::NODE_ID = "node_1";
  ActorRuntime::ExecutorSettings params = {.node_id = "node_1",
                                           .instance_id = "1"};
  Executors executors(
      std::thread(&ActorRuntime::LuaExecutor::os_task, &params),
      std::thread(&uActor::ActorRuntime::NativeExecutor::os_task, &params));
  sleep(1);

  uActor::ActorRuntime::ManagedNativeActor::register_actor_type<
      uActor::ActorRuntime::CodeStoreActor>("code_store");

  auto create_code_store = uActor::PubSub::Publication("node_1", "root", "1");
  create_code_store.set_attr("command", "spawn_native_actor");
  create_code_store.set_attr("spawn_actor_version", "default");
  create_code_store.set_attr("spawn_node_id", "node_1");
  create_code_store.set_attr("spawn_actor_type", "code_store");
  create_code_store.set_attr("spawn_instance_id", "1");
  create_code_store.set_attr("node_id", "node_1");
  create_code_store.set_attr("actor_type", "native_executor");
  create_code_store.set_attr("instance_id", "1");
  uActor::PubSub::Router::get_instance().publish(std::move(create_code_store));
  sleep(1);

  return std::move(executors);
}

TEST(RuntimeSystem, pingPong) {
  const char test_pong[] = R"(function receive(publication)
    if(publication.publisher_actor_type == "root") then
      publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "message", "pong"));
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto executors = start_executor_threads();

  spawn_actor(test_pong, "1");

  ASSERT_FALSE(root_handle.receive(0));

  auto ping = PubSub::Publication("node_1", "root", "1");
  ping.set_attr("node_id", "node_1");
  ping.set_attr("instance_id", "1");
  ping.set_attr("actor_type", "actor");
  ping.set_attr("message", "ping");
  PubSub::Router::get_instance().publish(std::move(ping));

  auto result = root_handle.receive(10000);
  ASSERT_TRUE(result);
  ASSERT_STREQ(
      std::get<std::string_view>(result->publication.get_attr("message"))
          .data(),
      "pong");

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, delayedSend) {
  const char delayed_pong[] = R"(function receive(publication)
    if(publication.publisher_actor_type == "root") then
      delayed_publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "message", "pong1"), 100);
      publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "message", "pong2"));
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto executors = start_executor_threads();

  spawn_actor(delayed_pong, "1");

  ASSERT_FALSE(root_handle.receive(0));

  auto ping = PubSub::Publication("node_1", "root", "1");
  ping.set_attr("node_id", "node_1");
  ping.set_attr("instance_id", "1");
  ping.set_attr("actor_type", "actor");
  ping.set_attr("message", "ping");
  PubSub::Router::get_instance().publish(std::move(ping));
  {
    auto result = root_handle.receive(10000);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong2");
  }
  usleep(200000);
  {
    auto result = root_handle.receive(10000);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong1");
  }

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, block_for) {
  const char block_for_pong[] = R"(function receive(publication)
    if(not(publication.publisher_actor_type == "lua_executor")) then
      if(publication["_internal_timeout"] == "_timeout") then
        publish(Publication.new("instance_id", "1", "node_id", node_id, "actor_type", "root", "message", "pong_timeout"));
      elseif(publication.message == "ping1") then
        deferred_block_for({foo="bar"}, 100)
        publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "message", "pong1"));
      elseif(publication.message == "ping2") then
        publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "message", "pong2"));
      end
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto executors = start_executor_threads();

  spawn_actor(block_for_pong, "1");

  ASSERT_FALSE(root_handle.receive(0));

  auto ping = PubSub::Publication("node_1", "root", "1");
  ping.set_attr("node_id", "node_1");
  ping.set_attr("instance_id", "1");
  ping.set_attr("actor_type", "actor");
  ping.set_attr("message", "ping1");
  PubSub::Router::get_instance().publish(std::move(ping));

  auto ping2 = PubSub::Publication("node_1", "root", "1");
  ping2.set_attr("node_id", "node_1");
  ping2.set_attr("instance_id", "1");
  ping2.set_attr("actor_type", "actor");
  ping2.set_attr("message", "ping2");
  PubSub::Router::get_instance().publish(std::move(ping2));

  {
    auto result = root_handle.receive(10000);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong1");
  }
  {
    auto result = root_handle.receive(10000);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong_timeout");
  }
  {
    auto result = root_handle.receive(10000);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong2");
  }

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, sub_unsub) {
  const char block_for_pong[] = R"(function receive(publication)
    if(publication.publisher_actor_type == "root") then
      if(publication.message == "sub") then
        sub_id = subscribe({foo="bar"})
        publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "sub_id", sub_id));
      elseif(publication.message == "unsub") then
        unsubscribe(publication.sub_id)
      else
        publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "message", "pong"));
      end
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto executors = start_executor_threads();

  spawn_actor(block_for_pong, "1");

  ASSERT_FALSE(root_handle.receive(0));

  auto sub = PubSub::Publication("node_1", "root", "1");
  sub.set_attr("node_id", "node_1");
  sub.set_attr("instance_id", "1");
  sub.set_attr("actor_type", "actor");
  sub.set_attr("message", "sub");
  PubSub::Router::get_instance().publish(std::move(sub));

  auto sub_result = root_handle.receive(10000);
  ASSERT_TRUE(sub_result);
  int32_t sub_id =
      std::get<std::int32_t>(sub_result->publication.get_attr("sub_id"));

  auto test_message = PubSub::Publication("node_1", "root", "1");
  test_message.set_attr("foo", "bar");
  test_message.set_attr("message", "asdf");
  PubSub::Router::get_instance().publish(std::move(test_message));

  {
    auto result = root_handle.receive(10000);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong");
  }

  auto unsub = PubSub::Publication("node_1", "root", "1");
  unsub.set_attr("node_id", "node_1");
  unsub.set_attr("instance_id", "1");
  unsub.set_attr("actor_type", "actor");
  unsub.set_attr("message", "unsub");
  unsub.set_attr("sub_id", sub_id);
  PubSub::Router::get_instance().publish(std::move(unsub));

  auto test_message2 = PubSub::Publication("node_1", "root", "1");
  test_message2.set_attr("foo", "bar");
  test_message2.set_attr("message", "asdf");
  PubSub::Router::get_instance().publish(std::move(test_message2));

  ASSERT_FALSE(root_handle.receive(2000));

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, complex_subscription) {
  const char block_for_pong[] = R"(function receive(publication)
    if(publication.publisher_actor_type == "root") then
      if(publication.message == "sub") then
        sub_id = subscribe({foo=1, bar={LT, 50.0}})
        publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "sub_id", sub_id));
      else
        publish(Publication.new("instance_id", publication.publisher_instance_id, "node_id", publication.publisher_node_id, "actor_type", publication.publisher_actor_type, "message", "pong"));
      end
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto executors = start_executor_threads();

  spawn_actor(block_for_pong, "1");

  ASSERT_FALSE(root_handle.receive(2000));

  auto sub = PubSub::Publication("node_1", "root", "1");
  sub.set_attr("node_id", "node_1");
  sub.set_attr("instance_id", "1");
  sub.set_attr("actor_type", "actor");
  sub.set_attr("message", "sub");
  PubSub::Router::get_instance().publish(std::move(sub));

  usleep(1000);

  auto test_message = PubSub::Publication("node_1", "root", "1");
  test_message.set_attr("foo", 1);
  test_message.set_attr("bar", 20.0f);
  PubSub::Router::get_instance().publish(std::move(test_message));

  auto sub_result = root_handle.receive(2000);
  ASSERT_TRUE(sub_result);

  {
    auto result = root_handle.receive(2000);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong");
  }

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, lifetime_messages) {
  const char test_pong[] = R"(function receive(publication)
end)";

  auto lifetime_handle = PubSub::Router::get_instance().new_subscriber();

  auto executors = start_executor_threads();

  PubSub::Filter primary_filter{
      PubSub::Constraint("category", "actor_lifetime")};
  lifetime_handle.subscribe(primary_filter, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  usleep(1000);

  spawn_actor(test_pong, "1");

  auto spawn_result = lifetime_handle.receive(2000);
  ASSERT_TRUE(spawn_result);
  ASSERT_STREQ(
      std::get<std::string_view>(spawn_result->publication.get_attr("type"))
          .data(),
      "actor_creation");

  auto exit = PubSub::Publication("node_1", "root", "1");
  exit.set_attr("node_id", "node_1");
  exit.set_attr("instance_id", "1");
  exit.set_attr("actor_type", "actor");
  exit.set_attr("type", "exit");
  PubSub::Router::get_instance().publish(std::move(exit));

  auto exit_result = lifetime_handle.receive(5000);
  ASSERT_TRUE(exit_result);
  ASSERT_STREQ(
      std::get<std::string_view>(exit_result->publication.get_attr("type"))
          .data(),
      "actor_exit");
  ASSERT_STREQ(std::get<std::string_view>(
                   exit_result->publication.get_attr("exit_reason"))
                   .data(),
               "clean_exit");

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, spawn_failure_syntax) {
  const char test_spawn_failure[] = R"(
    function receive(publication)
    asdf += 1
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  root_handle.subscribe(
      PubSub::Filter{PubSub::Constraint{"type", "actor_exit"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto executors = start_executor_threads();

  spawn_actor(test_spawn_failure, "1");

  auto result = root_handle.receive(3000);
  ASSERT_TRUE(result);
  EXPECT_STREQ(
      std::get<std::string_view>(result->publication.get_attr("exit_reason"))
          .data(),
      "initialization_failure");

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, spawn_failure_no_receive) {
  const char test_spawn_failure[] = R"(
    function foo(publication)
    asdf = 1
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  root_handle.subscribe(
      PubSub::Filter{PubSub::Constraint{"type", "actor_exit"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto executors = start_executor_threads();

  spawn_actor(test_spawn_failure, "1");

  auto result = root_handle.receive(3000);
  ASSERT_TRUE(result);
  EXPECT_STREQ(
      std::get<std::string_view>(result->publication.get_attr("exit_reason"))
          .data(),
      "initialization_failure");

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, spawn_failure_bad_call) {
  const char test_spawn_failure[] = R"(
    foo("bar")
    function receive(publication)
    asdf = 1
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  root_handle.subscribe(
      PubSub::Filter{PubSub::Constraint{"type", "actor_exit"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto executors = start_executor_threads();

  spawn_actor(test_spawn_failure, "1");

  auto result = root_handle.receive(3000);
  ASSERT_TRUE(result);
  EXPECT_STREQ(
      std::get<std::string_view>(result->publication.get_attr("exit_reason"))
          .data(),
      "initialization_failure");

  shutdown_executors(&executors);
}

TEST(RuntimeSystem, runtime_failure) {
  const char test_spawn_failure[] = R"(
  function receive(publication)
    asdf()
  end)";
  auto root_handle = subscription_handle_with_default_subscription();
  root_handle.subscribe(
      PubSub::Filter{PubSub::Constraint{"type", "actor_exit"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto executors = start_executor_threads();

  spawn_actor(test_spawn_failure, "1");

  auto result = root_handle.receive(3000);
  ASSERT_TRUE(result);
  EXPECT_STREQ(
      std::get<std::string_view>(result->publication.get_attr("exit_reason"))
          .data(),
      "runtime_failure");

  shutdown_executors(&executors);
}

}  // namespace uActor::Test
