#include <gtest/gtest.h>

#include <iostream>

#include "pubsub/router.hpp"
#include "pubsub/publication_factory.hpp"

namespace uActor::Test {

using PubSub::Filter, PubSub::Router, PubSub::Constraint, PubSub::Publication,
    PubSub::ConstraintPredicates;

TEST(ROUTERV3, base) {
  Router router{};
  auto r = router.new_subscriber();

  Filter primary_filter{
      Constraint(std::string("node_id"), "master_node"),
      Constraint(std::string("actor_type"), "master_type"),
      Constraint(std::string("instance_id"), "master_instance")};
  size_t master_subscription_id = r.subscribe(primary_filter, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

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
      Filter{Constraint{"foo", "bar"}, Constraint{"asdf", "ghjkl", ConstraintPredicates::Predicate::EQ, true}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

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

TEST(ROUTERV3, sub_all) {
  Router router{};
  auto r = router.new_subscriber();
  size_t sub_id = r.subscribe(Filter{}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto result0 = r.receive(0);
  ASSERT_TRUE(result0);
  ASSERT_TRUE(result0->publication.has_attr("type"));
  ASSERT_STREQ(std::get<std::string_view>(
                  result0->publication.get_attr("type"))
                  .data(),
              "local_subscription_added");

  result0 = r.receive(0);
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
  size_t sub_id = r.subscribe(Filter{Constraint{"test_number", 1}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

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
  }, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

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
  size_t sub_id = r.subscribe(Filter{Constraint{"test_number", 1.0f}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

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
  }, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

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
  }, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("test_number_less", 60.0f);
  router.publish(std::move(p));

  auto result1 = r.receive(100);
  ASSERT_FALSE(result1);
}

TEST(ROUTERV3, map_valid) {
  Router router{}; 
  auto r = router.new_subscriber();
  size_t sub_id = r.subscribe(Filter{
    Constraint("test_map", std::vector<Constraint>{Constraint("foo", "bar")}, ConstraintPredicates::Predicate::MAP_ALL)
  }, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);


  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  auto nested = std::make_shared<Publication::Map>();
  nested->set_attr("foo", "bar");
  p.set_attr("test_map", std::move(nested));
  router.publish(std::move(p));

  auto result1 = r.receive(100);
  ASSERT_TRUE(result1); 
}

  
TEST(ROUTERV3, map_invalid) {
  Router router{}; 
  auto r = router.new_subscriber();
  size_t sub_id = r.subscribe(Filter{
    Constraint("test_map", std::vector<Constraint>{Constraint("foo", "bar")}, ConstraintPredicates::Predicate::MAP_ALL)
  }, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);


  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  auto nested = std::make_shared<Publication::Map>();
  nested->set_attr("bar", "baz");
  p.set_attr("test_map", std::move(nested));
  router.publish(std::move(p));

  auto result1 = r.receive(100);
  ASSERT_FALSE(result1); 
}

TEST(ROUTERV3, valid_optional) {
  Router router{};
  auto r = router.new_subscriber();
  r.subscribe(Filter{Constraint{"opt", "ional",  ConstraintPredicates::Predicate::EQ, true}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto result0 = r.receive(0);
  ASSERT_TRUE(result0);
  result0 = r.receive(0);
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
  r.subscribe(Filter{Constraint{"asdf", "bar"}, Constraint{"opt", "ional", ConstraintPredicates::Predicate::EQ, true}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  auto result0 = r.receive(0);
  ASSERT_FALSE(result0);

  Publication p = Publication("sender_node", "sender_type", "sender_instance");
  p.set_attr("opt", "nope");
  p.set_attr("asdf", "bar");
  router.publish(std::move(p));

  auto result1 = r.receive(0);
  ASSERT_FALSE(result1);
}

TEST(ROUTERV3, unsubscribe) {
  Router router{};
  auto r = router.new_subscriber();

  uint32_t sub_id = r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
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

TEST(ROUTERV3, arg_fetch_policy_future) {
  Router router{};
  auto r = router.new_subscriber();

  auto sub_receiver = router.new_subscriber();
  sub_receiver.subscribe({Filter{Constraint{"type", "local_subscription_added"}, Constraint{"subscription_arguments", std::vector<Constraint>{Constraint("fetch_policy", "FUTURE")}}}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  while(sub_receiver.receive(0)) {
    // NOOP
  }

  auto args = PubSub::SubscriptionArguments();
  args.fetch_policy = PubSub::FetchPolicy::FUTURE;
  r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"), BoardFunctions::NODE_ID, std::move(args));

  auto opt_sub_msg = sub_receiver.receive(100);
  ASSERT_TRUE(opt_sub_msg);
  ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
  ASSERT_FALSE(opt_sub_msg->publication.has_attr("include_node_id"));

  Publication p{"sender_n", "sender_a", "sender_i"};
  p.set_attr("foo", "bar");
  router.publish(std::move(p));

  ASSERT_TRUE(r.receive(100));
}

TEST(ROUTERV3, arg_fetch_policy_future_remote_reflection_handling) {
  Router router{};
  auto r = router.new_subscriber();
  auto r2 = router.new_subscriber();
  auto r3 = router.new_subscriber();

  auto sub_receiver = router.new_subscriber();
  sub_receiver.subscribe({Filter{Constraint{"type", "local_subscription_added"}, Constraint{"subscription_arguments", std::vector<Constraint>{Constraint("fetch_policy", "FUTURE")}}}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  while(sub_receiver.receive(0)) {
    // NOOP
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FUTURE;
    r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"), "subscriber_node1", std::move(args));

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_EQ(opt_sub_msg->publication.get_str_attr("exclude_node_id"), "subscriber_node1");
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("include_node_id"));
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FUTURE;
    r2.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "2"), "subscriber_node2", std::move(args)); 

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_EQ(opt_sub_msg->publication.get_str_attr("include_node_id"), "subscriber_node1");
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FUTURE;
    r3.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "3"), "subscriber_node3", std::move(args)); 

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_FALSE(opt_sub_msg);
  }
}

TEST(ROUTERV3, arg_fetch_policy_future_local_reflection_handling) {
  Router router{};
  auto r = router.new_subscriber();
  auto r2 = router.new_subscriber();
  auto r3 = router.new_subscriber();

  auto sub_receiver = router.new_subscriber();
  sub_receiver.subscribe({Filter{Constraint{"type", "local_subscription_added"}, Constraint{"subscription_arguments", std::vector<Constraint>{Constraint("fetch_policy", "FUTURE")}}}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  while(sub_receiver.receive(0)) {
    // NOOP
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FUTURE;
    r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"), BoardFunctions::NODE_ID, std::move(args));

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("include_node_id"));
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FUTURE;
    r2.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "2"), BoardFunctions::NODE_ID, std::move(args)); 

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_EQ(opt_sub_msg->publication.get_str_attr("include_node_id"), BoardFunctions::NODE_ID);
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FUTURE;
    r3.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "3"), BoardFunctions::NODE_ID, std::move(args)); 

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_EQ(opt_sub_msg->publication.get_str_attr("include_node_id"), BoardFunctions::NODE_ID);
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
  }
}

TEST(ROUTERV3, arg_fetch_policy_fetch) {
  Router router{};
  auto r = router.new_subscriber();

  auto sub_receiver = router.new_subscriber();
  sub_receiver.subscribe({Filter{Constraint{"type", "local_subscription_added"}, Constraint{"subscription_arguments", std::vector<Constraint>{Constraint("fetch_policy", "FETCH")}}}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  while(sub_receiver.receive(0)) {
    // NOOP
  }

  auto args = PubSub::SubscriptionArguments();
  args.fetch_policy = PubSub::FetchPolicy::FETCH;
  r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"), BoardFunctions::NODE_ID, std::move(args));


  auto rec = sub_receiver.receive(100);
  ASSERT_TRUE(rec);

  Publication p{"sender_n", "sender_a", "sender_i"};
  p.set_attr("foo", "bar");
  router.publish(std::move(p));

  ASSERT_FALSE(r.receive(100));
}

TEST(ROUTERV3, arg_fetch_policy_fetch_future) {
  Router router{};
  auto r = router.new_subscriber();

  auto sub_receiver = router.new_subscriber();
  sub_receiver.subscribe({Filter{Constraint{"type", "local_subscription_added"}, Constraint{"subscription_arguments", std::vector<Constraint>{Constraint("fetch_policy", "FETCH_FUTURE")}}}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  while(sub_receiver.receive(0)) {
    // NOOP
  }

  auto args = PubSub::SubscriptionArguments();
  args.fetch_policy = PubSub::FetchPolicy::FETCH_FUTURE;
  r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"), BoardFunctions::NODE_ID, std::move(args));

  ASSERT_TRUE(sub_receiver.receive(100));

  Publication p{"sender_n", "sender_a", "sender_i"};
  p.set_attr("foo", "bar");
  router.publish(std::move(p));

  ASSERT_TRUE(r.receive(100));
}

TEST(ROUTERV3, arg_fetch_policy_fetch_future_remote_reflection_handling) {
  Router router{};
  auto r = router.new_subscriber();
  auto r2 = router.new_subscriber();
  auto r3 = router.new_subscriber();

  auto sub_receiver = router.new_subscriber();
  sub_receiver.subscribe({Filter{Constraint{"type", "local_subscription_added"}, Constraint{"subscription_arguments", std::vector<Constraint>{Constraint("fetch_policy", "FETCH_FUTURE")}}}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  while(sub_receiver.receive(0)) {
    // NOOP
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FETCH_FUTURE;
    r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"), "subscriber_node_1", std::move(args));

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_EQ(opt_sub_msg->publication.get_str_attr("exclude_node_id"), "subscriber_node_1");
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("include_node_id"));
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FETCH_FUTURE;
    r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "2"), "subscriber_node_2", std::move(args));

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    // TODO (raphaelhetzel) possibly change this.
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
    ASSERT_EQ(opt_sub_msg->publication.get_str_attr("include_node_id"), "subscriber_node_1");
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FETCH_FUTURE;
    r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "3"), "subscriber_node_3", std::move(args));

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_EQ(opt_sub_msg->publication.get_str_attr("exclude_node_id"), "subscriber_node_3");
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("include_node_id"));
  }
}

TEST(ROUTERV3, arg_fetch_policy_fetch_future_local_reflection_handling) {
  Router router{};
  auto r = router.new_subscriber();
  auto r2 = router.new_subscriber();
  auto r3 = router.new_subscriber();

  auto sub_receiver = router.new_subscriber();
  sub_receiver.subscribe({Filter{Constraint{"type", "local_subscription_added"}, Constraint{"subscription_arguments", std::vector<Constraint>{Constraint("fetch_policy", "FETCH_FUTURE")}}}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  while(sub_receiver.receive(0)) {
    // NOOP
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FETCH_FUTURE;
    r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"), BoardFunctions::NODE_ID, std::move(args));

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("include_node_id"));
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FETCH_FUTURE;
    r2.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "2"), BoardFunctions::NODE_ID, std::move(args));

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("include_node_id"));
  }

  {
    auto args = PubSub::SubscriptionArguments();
    args.fetch_policy = PubSub::FetchPolicy::FETCH_FUTURE;
    r3.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "3"), BoardFunctions::NODE_ID, std::move(args));

    auto opt_sub_msg = sub_receiver.receive(100);
    ASSERT_TRUE(opt_sub_msg);
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("exclude_node_id"));
    ASSERT_FALSE(opt_sub_msg->publication.has_attr("include_node_id"));
  }
}

TEST(ROUTERV3, subscription_ids) {
  Router router{};
  auto r = router.new_subscriber();
  auto r2 = router.new_subscriber();

  size_t r11 = r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  size_t r21 = r.subscribe(Filter{Constraint{"foo", "bar"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  size_t r12 = r.subscribe(Filter{Constraint{"bar", "baz"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));
  size_t r22 = r.subscribe(Filter{Constraint{"bar", "baz"}}, PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "test", "1"));

  ASSERT_LT(r11, r12);
  ASSERT_LT(r21, r22);
}

TEST(Publication, msgpack) {
  Publication p{"sender_node", "sender_actor_type", "sender_instance"};
  p.set_attr("string", "string");
  p.set_attr("integer", 1);
  p.set_attr("float", 2.0f);
  
  auto elem_handle = std::make_shared<Publication::Map>();

  elem_handle->set_attr("foo", "bar");

  p.set_attr("nested", elem_handle);

  auto serialized = p.to_msg_pack();

  auto pub_fac = PubSub::PublicationFactory();
  pub_fac.write(serialized->data()+4, serialized->size()-4);
  auto deserialized = pub_fac.build();
  ASSERT_TRUE(deserialized);
  Publication decoded = *deserialized;

  auto attr = deserialized->get_nested_component("nested");
  ASSERT_TRUE(attr);
  auto nested_attr = (*attr)->get_str_attr("foo");
  ASSERT_TRUE(nested_attr);
  ASSERT_EQ(*nested_attr, "bar");

  ASSERT_TRUE(p == decoded);
}

TEST(Publication, copy_on_write) {

  Publication p{};

  p.set_attr("foo", "bar");

  Publication p2{p};

  ASSERT_TRUE(p.is_shallow_copy());
  ASSERT_TRUE(p2.is_shallow_copy());

  p2.set_attr("foo", "baz");

  // TODO(raphaelhetzel) While this is the current expected behaviour,
  // this is not ideal as it creates an additional copy
  ASSERT_TRUE(p.is_shallow_copy());
  ASSERT_FALSE(p2.is_shallow_copy());

  auto p1_foo = p.get_str_attr("foo");
  ASSERT_TRUE(p1_foo);
  ASSERT_EQ(*p1_foo, "bar");

  auto p2_foo = p2.get_str_attr("foo");
  ASSERT_TRUE(p2_foo);
  ASSERT_EQ(*p2_foo, "baz");

}

TEST(Publication, nested_copy_on_write) {

  auto nested_elem = std::make_shared<Publication::Map>();
  nested_elem->set_attr("baz", "foo");

  Publication p{};

  p.set_attr("foo", nested_elem);;

  Publication p2{p};

  // COW is disabled when there are nested attributes
  ASSERT_FALSE(p.is_shallow_copy());
  ASSERT_FALSE(p2.is_shallow_copy());

  ASSERT_TRUE(p2.has_attr("foo"));

  auto elem_handle = p2.get_nested_component("foo");
  (*elem_handle)->set_attr("zap", "buzz");

  ASSERT_FALSE(p.is_shallow_copy());
  ASSERT_FALSE(p2.is_shallow_copy());

  auto p1_foo = p.get_nested_component("foo");
  ASSERT_TRUE(p1_foo);
  ASSERT_FALSE((*p1_foo)->has_attr("zap"));

  auto p2_foo = p2.get_nested_component("foo");
  ASSERT_TRUE(p2_foo);
  ASSERT_TRUE((*p2_foo)->has_attr("zap"));
  ASSERT_EQ((*p2_foo)->get_str_attr("zap"), "buzz");

}
TEST(Publication, map_orphan) {
  Publication p{};

  {
    auto nested_elem = std::make_shared<Publication::Map>();
    nested_elem->set_attr("foo", "bar");
    p.set_attr("nested", nested_elem);
  }

  ASSERT_TRUE(p.has_attr("nested"));
  auto elem_ref = *p.get_nested_component("nested");
  ASSERT_EQ(elem_ref->get_str_attr("foo"), "bar");

  p = Publication{};
  ASSERT_FALSE(p.has_attr("nested"));
  
  ASSERT_EQ(elem_ref->get_str_attr("foo"), "bar");
  elem_ref->set_attr("fizz", "buzz");
  ASSERT_EQ(elem_ref->get_str_attr("fizz"), "buzz"); 

}

TEST(Publication, orphan_after_cow) {
  Publication p{};

  {
    auto nested_elem = std::make_shared<Publication::Map>();
    nested_elem->set_attr("foo", "bar");
    p.set_attr("nested", nested_elem);
  }

  auto elem_ref = *p.get_nested_component("nested");
  Publication p2{p};

  ASSERT_TRUE(p.has_attr("nested"));
  auto elem_ref2 = *p2.get_nested_component("nested");

  p = Publication{};
  p2 = Publication{};

  elem_ref->set_attr("fizz", "buzz");
  ASSERT_EQ(elem_ref->get_str_attr("fizz"), "buzz");
  ASSERT_FALSE(elem_ref2->has_attr("fizz"));
}

TEST(Filter, map_serialization) {

  PubSub::Filter f{PubSub::Constraint("hello", "world"), PubSub::Constraint("foo", 1.0f, PubSub::ConstraintPredicates::Predicate::NE, true)};

  auto filter_serialized = f.to_publication_map();

  ASSERT_TRUE(filter_serialized->has_attr("hello"));
  auto hello_attr = *filter_serialized->get_nested_component("hello");
  ASSERT_FALSE(filter_serialized->has_attr("_m"));
  ASSERT_EQ(hello_attr->get_str_attr("operand"), "world");
  ASSERT_EQ(hello_attr->get_str_attr("operator"), "EQ");
  ASSERT_EQ(hello_attr->get_int_attr("optional"), 0);

  ASSERT_TRUE(filter_serialized->has_attr("foo"));
  auto foo_attr = *filter_serialized->get_nested_component("foo");
  ASSERT_FALSE(filter_serialized->has_attr("_m"));
  ASSERT_EQ(foo_attr->get_float_attr("operand"), 1.0f);
  ASSERT_EQ(foo_attr->get_str_attr("operator"), "NE");
  ASSERT_EQ(foo_attr->get_int_attr("optional"), 1);

  auto deserialized = *PubSub::Filter::from_publication_map(*filter_serialized);

  ASSERT_EQ(f, deserialized);
}

TEST(Filter, map_serialization_nested) {

  PubSub::Filter f{PubSub::Constraint("deep", std::vector<PubSub::Constraint>{PubSub::Constraint("foo", "bar"), PubSub::Constraint{"baz", "qux"}}, PubSub::ConstraintPredicates::Predicate::MAP_ALL)};

  auto filter_serialized = f.to_publication_map();

  auto deep = filter_serialized->get_nested_component("deep");
  ASSERT_TRUE(deep);
  ASSERT_EQ((*deep)->get_str_attr("operator"), "MAP_ALL");
  
  auto deep_attributes = (*deep)->get_nested_component("operand");
  ASSERT_TRUE(deep_attributes);

  auto foo_attr = (*deep_attributes)->get_nested_component("foo");
  auto baz_attr = (*deep_attributes)->get_nested_component("baz");

  ASSERT_TRUE(foo_attr);
  ASSERT_TRUE((*foo_attr)->has_attr("operand"));
  ASSERT_TRUE(baz_attr);
  ASSERT_TRUE((*baz_attr)->has_attr("operand"));

  auto deserialized = *PubSub::Filter::from_publication_map(*filter_serialized);

  ASSERT_EQ(f, deserialized);
}



TEST(Filter, map_serialization_duplicate_key) {

  PubSub::Filter f{PubSub::Constraint("data", 1.0f, PubSub::ConstraintPredicates::Predicate::GT), PubSub::Constraint("data", 5.0f, PubSub::ConstraintPredicates::Predicate::LT), PubSub::Constraint("data", 3.0f, PubSub::ConstraintPredicates::Predicate::NE)};

  auto filter_serialized =  f.to_publication_map();

  ASSERT_TRUE(filter_serialized->has_attr("data"));

  auto data_attr = *filter_serialized->get_nested_component("data");

  ASSERT_FALSE(data_attr->has_attr("operand"));
  ASSERT_EQ(data_attr->get_int_attr("_m"), 1);
  
  auto item_1 = data_attr->get_nested_component("0");
  auto item_2 = data_attr->get_nested_component("1");
  auto item_3 = data_attr->get_nested_component("2");

  ASSERT_TRUE(item_1);
  ASSERT_TRUE(item_2);
  ASSERT_TRUE(item_3);
  ASSERT_EQ((*item_1)->get_float_attr("operand"), 1.0f);
  ASSERT_EQ((*item_2)->get_float_attr("operand"), 5.0f);
  ASSERT_EQ((*item_3)->get_float_attr("operand"), 3.0f);

  auto deserialized = *PubSub::Filter::from_publication_map(*filter_serialized);

  ASSERT_EQ(f, deserialized);
}

}  // namespace uActor::Test
