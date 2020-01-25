#include <gtest/gtest.h>

#include <iostream>

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
  ASSERT_STREQ(std::get<std::string_view>(
                   result1->publication.get_attr("publisher_node_id"))
                   .data(),
               "sender_node");
  ASSERT_STREQ(
      std::get<std::string_view>(result1->publication.get_attr("node_id"))
          .data(),
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
  ASSERT_STREQ(std::get<std::string_view>(
                   result1->publication.get_attr("publisher_node_id"))
                   .data(),
               "sender_node");
  ASSERT_STREQ(
      std::get<std::string_view>(result1->publication.get_attr("foo")).data(),
      "bar");
}

TEST(ROUTERV3, integer_simple) {
  Router router{};
  auto r = router.new_subscriber();
  size_t sub_id = r.subscribe(Filter{Constraint{"test_number", 1}});

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("test_number", 1);
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_TRUE(result1);
  ASSERT_EQ(result1->subscription_id, sub_id);
  ASSERT_STREQ(std::get<std::string_view>(
                   result1->publication.get_attr("publisher_node_id"))
                   .data(),
               "sender_node");
  ASSERT_EQ(std::get<int32_t>(result1->publication.get_attr("test_number")), 1);
}

TEST(ROUTERV3, integer_advanced) {
  Router router{};
  auto r = router.new_subscriber();
  size_t sub_id = r.subscribe(Filter{
      Constraint{"test_number_less", 50, ConstraintPredicates::Predicate::LT},
      Constraint{"test_number_greater", 50,
                 ConstraintPredicates::Predicate::GT},
      Constraint{"test_number_greater_equal_equal", 50,
                 ConstraintPredicates::Predicate::GE},
      Constraint{"test_number_greater_equal_greater", 50,
                 ConstraintPredicates::Predicate::GE},
      Constraint{"test_number_less_equal_equal", 50,
                 ConstraintPredicates::Predicate::LE},
      Constraint{"test_number_less_equal_less", 50,
                 ConstraintPredicates::Predicate::LE},
  });

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("test_number_less", 1);
  p.set_attr("test_number_greater", 60);
  p.set_attr("test_number_greater_equal_equal", 50);
  p.set_attr("test_number_greater_equal_greater", 60);
  p.set_attr("test_number_less_equal_equal", 50);
  p.set_attr("test_number_less_equal_less", 49);
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_TRUE(result1);
  ASSERT_EQ(result1->subscription_id, sub_id);
  ASSERT_STREQ(std::get<std::string_view>(
                   result1->publication.get_attr("publisher_node_id"))
                   .data(),
               "sender_node");
  ASSERT_EQ(
      std::get<int32_t>(result1->publication.get_attr("test_number_less")), 1);
}

TEST(ROUTERV3, float_simple) {
  Router router{};
  auto r = router.new_subscriber();
  size_t sub_id = r.subscribe(Filter{Constraint{"test_number", 1.0f}});

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("test_number", 1.0f);
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_TRUE(result1);
  ASSERT_EQ(result1->subscription_id, sub_id);
  ASSERT_STREQ(std::get<std::string_view>(
                   result1->publication.get_attr("publisher_node_id"))
                   .data(),
               "sender_node");
  ASSERT_EQ(std::get<float>(result1->publication.get_attr("test_number")),
            1.0f);
}

TEST(ROUTERV3, float_advanced) {
  Router router{};
  auto r = router.new_subscriber();
  size_t sub_id = r.subscribe(Filter{
      Constraint{"test_number_less", 50.0f,
                 ConstraintPredicates::Predicate::LT},
      Constraint{"test_number_greater", 50.0f,
                 ConstraintPredicates::Predicate::GT},
      Constraint{"test_number_greater_equal_equal", 50.0f,
                 ConstraintPredicates::Predicate::GE},
      Constraint{"test_number_greater_equal_greater", 50.0f,
                 ConstraintPredicates::Predicate::GE},
      Constraint{"test_number_less_equal_equal", 50.0f,
                 ConstraintPredicates::Predicate::LE},
      Constraint{"test_number_less_equal_less", 50.0f,
                 ConstraintPredicates::Predicate::LE},
  });

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("test_number_less", 1.0f);
  p.set_attr("test_number_greater", 60.0f);
  p.set_attr("test_number_greater_equal_equal", 50.0f);
  p.set_attr("test_number_greater_equal_greater", 60.0f);
  p.set_attr("test_number_less_equal_equal", 50.0f);
  p.set_attr("test_number_less_equal_less", 49.0f);
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_TRUE(result1);
  ASSERT_EQ(result1->subscription_id, sub_id);
  ASSERT_STREQ(std::get<std::string_view>(
                   result1->publication.get_attr("publisher_node_id"))
                   .data(),
               "sender_node");
  ASSERT_EQ(std::get<float>(result1->publication.get_attr("test_number_less")),
            1.0f);
}

TEST(ROUTERV3, float_negative_match) {
  Router router{};
  auto r = router.new_subscriber();
  size_t sub_id = r.subscribe(Filter{
      Constraint{"test_number_less", 50.0f,
                 ConstraintPredicates::Predicate::LT},
  });

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("test_number_less", 60.0f);
  router.publish(std::move(p));

  auto result1 = r.receive(100);
  ASSERT_FALSE(result1);
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
  ASSERT_STREQ(std::get<std::string_view>(
                   result1->publication.get_attr("publisher_node_id"))
                   .data(),
               "sender_node");
  ASSERT_STREQ(
      std::get<std::string_view>(result1->publication.get_attr("opt")).data(),
      "ional");
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

TEST(Publication, msgpack) {
  Publication p{"sender_node", "sender_actor_type", "sender_instance"};
  p.set_attr("string", "string");
  p.set_attr("integer", 1);
  p.set_attr("float", 2.0f);

  std::string serialized = p.to_msg_pack();

  Publication decoded = *Publication::from_msg_pack(serialized);

  ASSERT_TRUE(p == decoded);
}
}  // namespace PubSub
