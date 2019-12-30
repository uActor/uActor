#include <gtest/gtest.h>

#include "router_v2.hpp"

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
