#include <gtest/gtest.h>
#include <unistd.h>

#include <thread>

#include "lua_runtime.hpp"
#include "publication.hpp"
#include "router_v2.hpp"
#include "subscription.hpp"

void spawn_actor(std::string_view code, std::string_view instance_id) {
  Publication create_actor = Publication("node_1", "root", "1");
  create_actor.set_attr("tag",
                        std::to_string(Tags::WELL_KNOWN_TAGS::SPAWN_LUA_ACTOR));
  create_actor.set_attr("spawn_code", code);
  create_actor.set_attr("spawn_node_id", "node_1");
  create_actor.set_attr("spawn_actor_type", "actor");
  create_actor.set_attr("spawn_instance_id", instance_id);
  create_actor.set_attr("node_id", "node_1");
  create_actor.set_attr("actor_type", "lua_runtime");
  create_actor.set_attr("instance_id", "1");
  PubSub::Router::get_instance().publish(std::move(create_actor));
}

void shutdown_runtime(std::thread* runtime_thread) {
  Publication exit = Publication("node_1", "root", "1");
  exit.set_attr("node_id", "node_1");
  exit.set_attr("instance_id", "1");
  exit.set_attr("actor_type", "lua_runtime");
  exit.set_attr("tag", std::to_string(Tags::WELL_KNOWN_TAGS::EXIT));
  PubSub::Router::get_instance().publish(std::move(exit));
  runtime_thread->join();
}

PubSub::SubscriptionHandle subscription_handle_with_default_subscription() {
  auto root_handle = PubSub::Router::get_instance().new_subscriber();
  PubSub::Filter primary_filter{
      PubSub::Constraint(std::string("node_id"), "node_1"),
      PubSub::Constraint(std::string("actor_type"), "root"),
      PubSub::Constraint(std::string("instance_id"), "1")};
  uint32_t root_subscription_id = root_handle.subscribe(primary_filter);
  return std::move(root_handle);
}

std::thread start_runtime_thread() {
  Params params = {.node_id = "node_1", .instance_id = "1"};
  std::thread runtime_thread = std::thread(&LuaRuntime::os_task, &params);
  usleep(100);
  return std::move(runtime_thread);
}

TEST(RuntimeSystem, pingPong) {
  const char test_pong[] = R"(function receive(publication)
    if(publication.sender_actor_type == "root") then
      send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, message="pong"});
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto runtime_thread = start_runtime_thread();

  spawn_actor(test_pong, "1");

  ASSERT_FALSE(root_handle.receive(100));

  Publication ping = Publication("node_1", "root", "1");
  ping.set_attr("node_id", "node_1");
  ping.set_attr("instance_id", "1");
  ping.set_attr("actor_type", "actor");
  ping.set_attr("message", "ping");
  PubSub::Router::get_instance().publish(std::move(ping));

  auto result = root_handle.receive(100);
  ASSERT_TRUE(result);
  ASSERT_STREQ(
      std::get<std::string_view>(result->publication.get_attr("message"))
          .data(),
      "pong");

  shutdown_runtime(&runtime_thread);
}

TEST(RuntimeSystem, delayedSend) {
  const char delayed_pong[] = R"(function receive(publication)
    if(publication.sender_actor_type == "root") then 
      delayed_publish({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, message="pong1"}, 100);
      send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, message="pong2"});
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto runtime_thread = start_runtime_thread();

  spawn_actor(delayed_pong, "1");

  ASSERT_FALSE(root_handle.receive(100));

  Publication ping = Publication("node_1", "root", "1");
  ping.set_attr("node_id", "node_1");
  ping.set_attr("instance_id", "1");
  ping.set_attr("actor_type", "actor");
  ping.set_attr("message", "ping");
  PubSub::Router::get_instance().publish(std::move(ping));
  {
    auto result = root_handle.receive(100);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong2");
  }
  usleep(200000);
  {
    auto result = root_handle.receive(100);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong1");
  }

  shutdown_runtime(&runtime_thread);
}

TEST(RuntimeSystem, block_for) {
  const char block_for_pong[] = R"(function receive(publication)
    if(not(publication.sender_actor_type == "lua_runtime")) then
      if(publication["_internal_timeout"] == "_timeout") then
        send({instance_id="1", node_id=node_id, actor_type="root", message="pong_timeout"});
      elseif(publication.message == "ping1") then
        deferred_block_for({foo="bar"}, 100)
        send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, message="pong1"});
      elseif(publication.message == "ping2") then
        send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, message="pong2"});
      end
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto runtime_thread = start_runtime_thread();

  spawn_actor(block_for_pong, "1");

  ASSERT_FALSE(root_handle.receive(100));

  Publication ping = Publication("node_1", "root", "1");
  ping.set_attr("node_id", "node_1");
  ping.set_attr("instance_id", "1");
  ping.set_attr("actor_type", "actor");
  ping.set_attr("message", "ping1");
  PubSub::Router::get_instance().publish(std::move(ping));

  Publication ping2 = Publication("node_1", "root", "1");
  ping2.set_attr("node_id", "node_1");
  ping2.set_attr("instance_id", "1");
  ping2.set_attr("actor_type", "actor");
  ping2.set_attr("message", "ping2");
  PubSub::Router::get_instance().publish(std::move(ping2));

  {
    auto result = root_handle.receive(100);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong1");
  }
  {
    auto result = root_handle.receive(300);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong_timeout");
  }
  {
    auto result = root_handle.receive(100);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong2");
  }

  shutdown_runtime(&runtime_thread);
}

TEST(RuntimeSystem, sub_unsub) {
  const char block_for_pong[] = R"(function receive(publication)
    if(publication.sender_actor_type == "root") then
      if(publication.message == "sub") then
        sub_id = subscribe({foo="bar"})
        send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, sub_id=sub_id});
      elseif(publication.message == "unsub") then
        unsubscribe(publication.sub_id)
      else
        send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, message="pong"});
      end
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto runtime_thread = start_runtime_thread();

  spawn_actor(block_for_pong, "1");

  ASSERT_FALSE(root_handle.receive(100));

  Publication sub = Publication("node_1", "root", "1");
  sub.set_attr("node_id", "node_1");
  sub.set_attr("instance_id", "1");
  sub.set_attr("actor_type", "actor");
  sub.set_attr("message", "sub");
  PubSub::Router::get_instance().publish(std::move(sub));

  usleep(10000);
  Publication test_message = Publication("node_1", "root", "1");
  test_message.set_attr("foo", "bar");
  test_message.set_attr("message", "asdf");
  PubSub::Router::get_instance().publish(std::move(test_message));

  auto sub_result = root_handle.receive(100);
  ASSERT_TRUE(sub_result);
  int32_t sub_id =
      std::get<std::int32_t>(sub_result->publication.get_attr("sub_id"));

  {
    auto result = root_handle.receive(100);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong");
  }

  Publication unsub = Publication("node_1", "root", "1");
  unsub.set_attr("node_id", "node_1");
  unsub.set_attr("instance_id", "1");
  unsub.set_attr("actor_type", "actor");
  unsub.set_attr("message", "unsub");
  unsub.set_attr("sub_id", sub_id);
  PubSub::Router::get_instance().publish(std::move(unsub));

  ASSERT_FALSE(root_handle.receive(100));

  shutdown_runtime(&runtime_thread);
}

TEST(RuntimeSystem, complex_subscription) {
  const char block_for_pong[] = R"(function receive(publication)
    if(publication.sender_actor_type == "root") then
      if(publication.message == "sub") then
        sub_id = subscribe({foo=1, bar={50.0, operators.LT}})
        send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, sub_id=sub_id});
      else
        send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, message="pong"});
      end
    end
  end)";

  auto root_handle = subscription_handle_with_default_subscription();
  auto runtime_thread = start_runtime_thread();

  spawn_actor(block_for_pong, "1");

  ASSERT_FALSE(root_handle.receive(100));

  Publication sub = Publication("node_1", "root", "1");
  sub.set_attr("node_id", "node_1");
  sub.set_attr("instance_id", "1");
  sub.set_attr("actor_type", "actor");
  sub.set_attr("message", "sub");
  PubSub::Router::get_instance().publish(std::move(sub));

  usleep(10000);

  Publication test_message = Publication("node_1", "root", "1");
  test_message.set_attr("foo", 1);
  test_message.set_attr("bar", 20.0f);
  PubSub::Router::get_instance().publish(std::move(test_message));

  auto sub_result = root_handle.receive(100);
  ASSERT_TRUE(sub_result);

  {
    auto result = root_handle.receive(100);
    ASSERT_TRUE(result);
    ASSERT_STREQ(
        std::get<std::string_view>(result->publication.get_attr("message"))
            .data(),
        "pong");
  }

  shutdown_runtime(&runtime_thread);
}
