#include <gtest/gtest.h>

#include "publication.hpp"
#include "subscription.hpp"

namespace PubSub {
TEST(ROUTERV3, base) {
  Router router{};
  auto r = router.new_subscriber();

  Filter primary_filter{
      Constraint(std::string("node_id"), "master_node"),
      Constraint(std::string("actor_type"), "master_type"),
      Constraint(std::string("instance_id"), "master_instance")};
  size_t master_subscription_id = r.subscribe(primary_filter);

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("node_id", "master_node");
  p.set_attr("actor_type", "master_type");
  p.set_attr("instance_id", "master_instance");
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_TRUE(result1);
  ASSERT_EQ(result1->subscription_id, master_subscription_id);
  ASSERT_STREQ((result1->publication.get_attr("sender_node_id"))->data(),
               "sender_node");
  ASSERT_STREQ((result1->publication.get_attr("node_id"))->data(),
               "master_node");
}

TEST(ROUTERV3, alias) {
  Router router{};
  auto r = router.new_subscriber();
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
  Router router{};
  auto r = router.new_subscriber();
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
  Router router{};
  auto r = router.new_subscriber();
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
  Router router{};
  auto r = router.new_subscriber();

  uint32_t sub_id = r.subscribe(Filter{Constraint{"foo", "bar"}});
  r.unsubscribe(sub_id);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("foo", "bar");
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_FALSE(result1);
}

TEST(ROUTERV3, queue_deleted_when_reference_out_of_scope) {
  Router router{};
  {
    auto r = router.new_subscriber();

    Publication p =
        Publication("sender_node", "sender_type", "sender_instance");
    p.set_attr("node_id", "master_node");
    p.set_attr("actor_type", "master_type");
    p.set_attr("instance_id", "master_instance");
    router.publish(std::move(p));
  }
  {
    auto r = router.new_subscriber();
    auto result1 = r.receive(0);
    ASSERT_FALSE(result1);
  }
}

TEST(ROUTERV3, subscription_ids) {
  Router router{};
  auto r = router.new_subscriber();
  auto r2 = router.new_subscriber();

  size_t r11 = r.subscribe(Filter{Constraint{"foo", "bar"}});
  size_t r21 = r.subscribe(Filter{Constraint{"foo", "bar"}});
  size_t r12 = r.subscribe(Filter{Constraint{"bar", "baz"}});
  size_t r22 = r.subscribe(Filter{Constraint{"bar", "baz"}});

  ASSERT_LT(r11, r12);
  ASSERT_LT(r21, r22);
}
}  // namespace PubSub
