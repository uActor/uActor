#include <gtest/gtest.h>

#include <thread>

#include "lua_runtime.hpp"
#include "publication.hpp"
#include "router_v2.hpp"
#include "subscription.hpp"

const char test_pong[] = R"(function receive(publication)
  send({instance_id=publication.sender_instance_id, node_id=publication.sender_node_id, actor_type=publication.sender_actor_type, message="pong", tag="1"});
end)";

TEST(RuntimeSystem, pingPong) {
  Params params = {.node_id = "node_1", .instance_id = "1"};
  std::thread runtime = std::thread(&LuaRuntime::os_task, &params);
  sleep(1);

  Publication create_actor = Publication("node_1", "root", "1");
  create_actor.set_attr("tag",
                        std::to_string(Tags::WELL_KNOWN_TAGS::SPAWN_LUA_ACTOR));
  create_actor.set_attr("spawn_code", test_pong);
  create_actor.set_attr("spawn_node_id", "node_1");
  create_actor.set_attr("spawn_actor_type", "actor");
  create_actor.set_attr("spawn_instance_id", "1");
  create_actor.set_attr("node_id", "node_1");
  create_actor.set_attr("actor_type", "lua_runtime");
  create_actor.set_attr("instance_id", "1");
  PubSub::Router::get_instance().publish(std::move(create_actor));

  sleep(1);

  auto root_handle = PubSub::Router::get_instance().new_subscriber();
  PubSub::Filter primary_filter{
      PubSub::Constraint(std::string("node_id"), "node_1"),
      PubSub::Constraint(std::string("actor_type"), "root"),
      PubSub::Constraint(std::string("instance_id"), "1")};
  size_t root_subscription_id = root_handle.subscribe(primary_filter);

  ASSERT_FALSE(root_handle.receive(0));

  Publication ping = Publication("node_1", "root", "1");
  ping.set_attr("node_id", "node_1");
  ping.set_attr("instance_id", "1");
  ping.set_attr("actor_type", "actor");
  ping.set_attr("tag", std::to_string(0));
  ping.set_attr("message", "ping");
  PubSub::Router::get_instance().publish(std::move(ping));
  sleep(1);
  auto result = root_handle.receive(0);
  ASSERT_TRUE(result);
  ASSERT_STREQ(result->publication.get_attr("message")->data(), "pong");

  sleep(5);

  Publication exit = Publication("node_1", "root", "1");
  exit.set_attr("node_id", "node_1");
  exit.set_attr("instance_id", "1");
  exit.set_attr("actor_type", "lua_runtime");
  exit.set_attr("tag", std::to_string(Tags::WELL_KNOWN_TAGS::EXIT));
  PubSub::Router::get_instance().publish(std::move(exit));
  printf("join\n");
  runtime.join();
  EXPECT_EQ(0, 0);
}

