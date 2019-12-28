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
  RouterV3::get_instance().publish(std::move(create_actor));

  sleep(1);

  auto root_handle =
      RouterV3::get_instance().register_master("node_1", "root", "1");
  ASSERT_FALSE(root_handle.receive(0));

  Publication ping = Publication("node_1", "root", "1");
  ping.set_attr("node_id", "node_1");
  ping.set_attr("instance_id", "1");
  ping.set_attr("actor_type", "actor");
  ping.set_attr("tag", std::to_string(0));
  ping.set_attr("message", "ping");
  RouterV3::get_instance().publish(std::move(ping));
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
  RouterV3::get_instance().publish(std::move(exit));
  printf("join\n");
  runtime.join();
  EXPECT_EQ(0, 0);
}

TEST(ROUTER, register_send_receive) {
  RouterV2::getInstance().register_actor("foo/bar/1");
  Message m = Message("asdf", "foo/bar/1", 1, "bar");
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/bar/1", 0));
  RouterV2::getInstance().send(std::move(m));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/bar/1", 0));

  RouterV2::getInstance().deregister_actor("foo/bar/1");
}

TEST(ROUTER, aliasing) {
  RouterV2::getInstance().register_actor("foo/bar/1");
  RouterV2::getInstance().register_alias("foo/bar/1", "baz");

  ASSERT_FALSE(RouterV2::getInstance().receive("baz", 0));
  Message m = Message("asdf", "baz", 1, "bar");
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/bar/1", 0));
  RouterV2::getInstance().send(std::move(m));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/bar/1", 0));

  RouterV2::getInstance().deregister_actor("foo/bar/1");
}

TEST(ROUTER, wildcard_plus) {
  RouterV2::getInstance().register_actor("foo/bar/1");
  RouterV2::getInstance().register_actor("foo/baz/1");
  RouterV2::getInstance().register_actor("foo/bar/2");

  Message topic1 = Message("asdf", "foo/+/1", 1, "bar");
  Message topic2 = Message("asdf", "foo/+/2", 1, "baz");

  RouterV2::getInstance().send(std::move(topic2));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/bar/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/baz/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/bar/2", 0));

  RouterV2::getInstance().send(std::move(topic1));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/bar/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/baz/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/bar/2", 0));

  RouterV2::getInstance().deregister_actor("foo/bar/1");
  RouterV2::getInstance().deregister_actor("foo/baz/1");
  RouterV2::getInstance().deregister_actor("foo/bar/2");
}

TEST(ROUTER, wildcard_plus_plus) {
  RouterV2::getInstance().register_actor("foo/room/bar/1");
  RouterV2::getInstance().register_actor("foo/room2/1");
  RouterV2::getInstance().register_actor("foo/room3/bar/2");

  Message topic1 = Message("asdf", "foo/+/bar/1", 1, "");
  Message topic2 = Message("asdf", "foo/+/+/2", 1, "");
  Message topic3 = Message("asdf", "foo/+/+/1", 1, "");
  Message topic4 = Message("asdf", "foo/room/+/+", 1, "");

  RouterV2::getInstance().send(std::move(topic1));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room/bar/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/room2/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/room3/bar/2", 0));

  RouterV2::getInstance().send(std::move(topic2));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/room/bar/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room2/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room3/bar/2", 0));

  RouterV2::getInstance().send(std::move(topic3));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room/bar/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room2/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/room3/bar/2", 0));

  RouterV2::getInstance().send(std::move(topic3));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room/bar/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room2/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/room3/bar/2", 0));

  RouterV2::getInstance().send(std::move(topic4));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room/bar/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/room2/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/room3/bar/2", 0));

  RouterV2::getInstance().deregister_actor("foo/room/bar/1");
  RouterV2::getInstance().deregister_actor("foo/room2/1");
  RouterV2::getInstance().deregister_actor("foo/room3/bar/2");
}

TEST(ROUTER, wildcard_hash) {
  RouterV2::getInstance().register_actor("foo/room/bar/1");
  RouterV2::getInstance().register_actor("foo/room2/1");
  RouterV2::getInstance().register_actor("baz/room/bar/1");

  Message topic1 = Message("asdf", "foo/#", 1, "");
  Message topic2 = Message("asdf", "#", 1, "");

  RouterV2::getInstance().send(std::move(topic1));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room/bar/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room2/1", 0));
  ASSERT_FALSE(RouterV2::getInstance().receive("baz/room/bar/1", 0));

  RouterV2::getInstance().send(std::move(topic2));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room/bar/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/room2/1", 0));
  ASSERT_TRUE(RouterV2::getInstance().receive("baz/room/bar/1", 0));

  RouterV2::getInstance().deregister_actor("foo/room/bar/1");
  RouterV2::getInstance().deregister_actor("foo/room2/1");
  RouterV2::getInstance().deregister_actor("baz/room/bar/1");
}

TEST(ROUTER, wildcard_path_creation) {
  RouterV2 router = RouterV2();
  router.register_actor("foo/bar/baz/1");
  Message m1 = Message("asdf", "foo/#", 1, "");
  Message m2 = Message("asdf", "foo/+/baz/1", 1, "");

  router.send(std::move(m1));
  auto result1 = router.receive("foo/bar/baz/1");
  ASSERT_TRUE(result1);
  ASSERT_STREQ(result1->receiver(), "foo/bar/baz/1");

  router.send(std::move(m2));
  auto result2 = router.receive("foo/bar/baz/1");
  ASSERT_TRUE(result2);
  ASSERT_STREQ(result2->receiver(), "foo/bar/baz/1");
}

TEST(ROUTERV3, base) {
  RouterV3 router{};
  auto r =
      router.register_master("master_node", "master_type", "master_instance");

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("node_id", "master_node");
  p.set_attr("actor_type", "master_type");
  p.set_attr("instance_id", "master_instance");
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_TRUE(result1);
  ASSERT_EQ(result1->subscription_id, r.master_subscription_id());
  ASSERT_STREQ((result1->publication.get_attr("sender_node_id"))->data(),
               "sender_node");
  ASSERT_STREQ((result1->publication.get_attr("node_id"))->data(),
               "master_node");
}

TEST(ROUTERV3, alias) {
  RouterV3 router{};
  auto r =
      router.register_master("master_node", "master_type", "master_instance");
  size_t sub_id = r.subscribe(
      Filter{Constraint{"foo", "bar"}, Constraint{"?asdf", "ghjkl"}});

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("foo", "bar");
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_TRUE(result1);
  ASSERT_EQ(result1->subscription_id, sub_id);
  ASSERT_STREQ((result1->publication.get_attr("sender_node_id"))->data(),
               "sender_node");
  ASSERT_STREQ((result1->publication.get_attr("foo"))->data(), "bar");
}

TEST(ROUTERV3, valid_optional) {
  RouterV3 router{};
  auto r =
      router.register_master("master_node", "master_type", "master_instance");
  r.subscribe(Filter{Constraint{"?opt", "ional"}});

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("opt", "ional");
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_TRUE(result1);
  ASSERT_STREQ((result1->publication.get_attr("sender_node_id"))->data(),
               "sender_node");
  ASSERT_STREQ((result1->publication.get_attr("opt"))->data(), "ional");
}

TEST(ROUTERV3, invalid_optional) {
  RouterV3 router{};
  auto r =
      router.register_master("master_node", "master_type", "master_instance");
  r.subscribe(Filter{Constraint{"?opt", "ional"}});

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("opt", "nope");
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_FALSE(result1);
}

TEST(ROUTERV3, unsubscribe) {
  RouterV3 router{};
  auto r =
      router.register_master("master_node", "master_type", "master_instance");

  r.subscribe(Filter{Constraint{"foo", "bar"}});
  r.unsubscribe(Filter{Constraint{"foo", "bar"}});

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("foo", "bar");
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_FALSE(result1);
}

TEST(ROUTERV3, queue_deleted_when_reference_out_of_scope) {
  RouterV3 router{};
  {
    auto r =
        router.register_master("master_node", "master_type", "master_instance");

    Publication p =
        Publication("sender_node", "sender_type", "sender_instance");
    p.set_attr("node_id", "master_node");
    p.set_attr("actor_type", "master_type");
    p.set_attr("instance_id", "master_instance");
    router.publish(std::move(p));
  }
  {
    auto r =
        router.register_master("master_node", "master_type", "master_instance");
    auto result1 = r.receive(0);
    ASSERT_FALSE(result1);
  }
}

TEST(ROUTERV3, subscription_ids) {
  RouterV3 router{};
  auto r =
      router.register_master("master_node", "master_type", "master_instance");
  auto r2 =
      router.register_master("master_node", "master_type", "master_instance");

  size_t r11 = r.subscribe(Filter{Constraint{"foo", "bar"}});
  size_t r21 = r.subscribe(Filter{Constraint{"foo", "bar"}});
  size_t r12 = r.subscribe(Filter{Constraint{"bar", "baz"}});
  size_t r22 = r.subscribe(Filter{Constraint{"bar", "baz"}});

  ASSERT_LT(r11, r12);
  ASSERT_LT(r21, r22);
}
