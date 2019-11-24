#include <gtest/gtest.h>
// #include <thread>

// #include <lua_runtime.hpp>
#include <router_v2.hpp>

// const char test_pong[] = R"(function receive(sender, tag, message)
//   send(sender, 0, "pong");
// end)";

// TEST(RuntimeSystem, pingPong) {
//   Params params = {.id = 1, .router = &RouterV2::getInstance()};
//   std::thread runtime = std::thread(&LuaRuntime::os_task, &params);
//   sleep(1);

//   Message create_actor = Message(sizeof(test_pong) + 8);
//   create_actor.sender(0);
//   create_actor.receiver(1);
//   create_actor.tag(Tags::WELL_KNOWN_TAGS::SPAWN_LUA_ACTOR);
//   *reinterpret_cast<uint64_t*>(create_actor.buffer()) = 2;
//   std::memcpy(create_actor.buffer() + 8, test_pong, sizeof(test_pong));
//   RouterV2::getInstance().send(0, 1, std::move(create_actor));

//   RouterV2::getInstance().register_actor(0);
//   ASSERT_FALSE(RouterV2::getInstance().receive(0, 0));

//   RouterV2::getInstance().send(0, 1, Message(0, 2, 0, "ping"));
//   sleep(1);
//   std::optional<Message> result = RouterV2::getInstance().receive(0, 0);
//   ASSERT_TRUE(result);
//   ASSERT_STREQ(result->ro_buffer(), "pong");

//   Message m = Message(0, 1, Tags::WELL_KNOWN_TAGS::EXIT, "");
//   RouterV2::getInstance().send(0, 1, std::move(m));
//   runtime.join();
//   EXPECT_EQ(0, 0);
// }

TEST(ROUTER, register_send_receive) {
  RouterV2::getInstance().register_actor("foo/bar/1");
  Message m = Message("asdf", "foo/bar/1", 1, "bar");
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/bar/1", 0));
  RouterV2::getInstance().send(std::move(m));
  ASSERT_TRUE(RouterV2::getInstance().receive("foo/bar/1", 0));

  RouterV2::getInstance().deregister_actor("foo/bar/1");
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/bar/1", 0));
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
  ASSERT_FALSE(RouterV2::getInstance().receive("foo/bar/1", 0));
}
