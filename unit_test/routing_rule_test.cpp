#include <gtest/gtest.h>

#include <iostream>

#include "remote/subscription_processors/node_local_filter_drop.hpp"
#include "remote/subscription_processors/node_id_aggregator.hpp"
#include "remote/subscription_processors/cluster_aggregator.hpp"
#include "remote/subscription_processors/cluster_barrier.hpp"
#include "remote/subscription_processors/optional_constraint_drop.hpp"

namespace uActor::Test {

using namespace uActor::Remote;
using namespace uActor::PubSub;

TEST(RoutingRuleLocalFilterDrop, DroppedRule) {
  auto processor = NodeLocalFilterDrop("n_local", "n_remote");

  auto filter = Filter{Constraint{"publisher_node_id", "n_local"}, Constraint{"foo", "bar"}};
  auto filter_copy = filter;

  ASSERT_TRUE(processor.process_added(&filter));
  ASSERT_EQ(filter, filter_copy);
  ASSERT_TRUE(processor.process_removed(&filter));
  ASSERT_EQ(filter, filter_copy);
}

TEST(RoutingRuleLocalFilterDrop, IgnoredRule) {
  auto processor = NodeLocalFilterDrop("n_local", "n_remote");

  auto filter = Filter{Constraint{"baz", "n_local"}, Constraint{"foo", "bar"}};
  auto filter_copy = filter;

  ASSERT_FALSE(processor.process_added(&filter));
  ASSERT_EQ(filter, filter_copy);
  ASSERT_FALSE(processor.process_removed(&filter));
  ASSERT_EQ(filter, filter_copy);
}

TEST(NodeIdAggregator, MatchingRules) {
  auto processor = NodeIdAggregator("n_local", "n_remote");

  auto filter = Filter{Constraint{"node_id", "n_local"}, Constraint{"other1", "other_value"}};
  auto filter_copy_add = filter;
  auto filter_copy_remove = filter;

  auto filter2 = Filter{Constraint{"node_id", "n_local"}, Constraint{"other2", "other_value"}};
  auto filter2_copy_add = filter2;
  auto filter2_copy_remove = filter2;

  ASSERT_FALSE(processor.process_added(&filter_copy_add));
  ASSERT_NE(filter_copy_add, filter);

  ASSERT_TRUE(processor.process_added(&filter2_copy_add));
  ASSERT_EQ(filter2_copy_add, filter2);

  ASSERT_TRUE(processor.process_removed(&filter2_copy_remove));
  ASSERT_EQ(filter2_copy_remove, filter2);

  ASSERT_FALSE(processor.process_removed(&filter_copy_remove));
  ASSERT_NE(filter_copy_remove, filter);
}

TEST(NodeIdAggregator, MatchingAndIgnoredRule) {
  auto processor = NodeIdAggregator("n_local", "n_remote");

  auto filter = Filter{Constraint{"node_id", "n_local"}, Constraint{"other1", "other_value"}};
  auto filter_copy_add = filter;
  auto filter_copy_remove = filter;

  auto filter2 = Filter{Constraint{"random", "n_local"}, Constraint{"other2", "other_value"}};
  auto filter2_copy_add = filter2;
  auto filter2_copy_remove = filter2;

  ASSERT_FALSE(processor.process_added(&filter_copy_add));
  ASSERT_NE(filter_copy_add, filter);
  ASSERT_EQ(filter_copy_add.serialize(), "node_id,s,EQ,n_local");

  ASSERT_FALSE(processor.process_added(&filter2_copy_add));
  ASSERT_EQ(filter2_copy_add, filter2);

  ASSERT_FALSE(processor.process_removed(&filter2_copy_remove));
  ASSERT_EQ(filter2_copy_remove, filter2);

  ASSERT_FALSE(processor.process_removed(&filter_copy_remove));
  ASSERT_NE(filter_copy_remove, filter);
  ASSERT_EQ(filter_copy_remove, filter_copy_add);
}

TEST(ClusterAggregator, SingleMatchingRule) {
  auto processor = ClusterAggregator("n_local", "n_remote", ClusterLabels{{"wing", ClusterLabelValuePair{"wing_1", "wing_2"}}});

  auto filter = Filter{Constraint{"wing", "wing_1"}, Constraint{"other1", "other_value"}};
  auto filter_copy_add = filter;
  auto filter_copy_remove = filter;

  ASSERT_FALSE(processor.process_added(&filter_copy_add));
  ASSERT_NE(filter_copy_add, filter);

  ASSERT_FALSE(processor.process_removed(&filter_copy_remove));
  ASSERT_NE(filter_copy_remove, filter);
  ASSERT_EQ(filter_copy_remove, filter_copy_add);
}

TEST(ClusterAggregator, IgnoredRule) {
  auto processor = ClusterAggregator("n_local", "n_remote", ClusterLabels{{"wing", ClusterLabelValuePair{"wing_1", "wing_2"}}, {"floor", ClusterLabelValuePair{"floor_1", "floor_2"}}});

  auto filter = Filter{Constraint{"wing", "wing_1"}, Constraint{"other", "floor_1"}, Constraint{"other1", "other_value"}};
  auto filter_copy_add = filter;
  auto filter_copy_remove = filter;

  ASSERT_FALSE(processor.process_added(&filter_copy_add));
  ASSERT_EQ(filter_copy_add, filter);

  ASSERT_FALSE(processor.process_removed(&filter_copy_remove));
  ASSERT_EQ(filter_copy_remove, filter);
}

TEST(ClusterAggregator, CoveredRule) {
  auto processor = ClusterAggregator("n_local", "n_remote", ClusterLabels{{"wing", ClusterLabelValuePair{"wing_1", "wing_2"}}, {"floor", ClusterLabelValuePair{"floor_1", "floor_2"}}});

  auto filter = Filter{Constraint{"wing", "wing_1"}, Constraint{"floor", "floor_1"}, Constraint{"other1", "other_value"}};
  auto filter_copy_add = filter;
  auto filter_copy_remove = filter;

  auto filter2 = Filter{Constraint{"wing", "wing_1"}, Constraint{"floor", "floor_1"}, Constraint{"other2", "other_value"}};
  auto filter2_copy_add = filter2;
  auto filter2_copy_remove = filter2;

  ASSERT_FALSE(processor.process_added(&filter_copy_add));
  ASSERT_NE(filter_copy_add, filter);

  ASSERT_TRUE(processor.process_added(&filter2_copy_add));
  ASSERT_EQ(filter2_copy_add, filter2);

  ASSERT_TRUE(processor.process_removed(&filter2_copy_remove));
  ASSERT_EQ(filter2_copy_remove, filter2); 

  ASSERT_FALSE(processor.process_removed(&filter_copy_remove));
  ASSERT_NE(filter_copy_remove, filter);
  ASSERT_EQ(filter_copy_remove, filter_copy_add);
}

TEST(ClusterBarrier, SingleMatchingRule) {
  auto processor = ClusterBarrier("n_local", "n_remote", ClusterLabels{{"wing", ClusterLabelValuePair{"wing_1", "wing_2"}}}, KeyList{"node_id"});

  auto filter = Filter{Constraint{"node_id", "n_n_n"}, Constraint{"other1", "other_value"}};
  auto filter_copy_add = filter;
  auto filter_copy_remove = filter;

  ASSERT_TRUE(processor.process_added(&filter_copy_add));
  ASSERT_EQ(filter_copy_add, filter);

  ASSERT_TRUE(processor.process_removed(&filter_copy_remove));
  ASSERT_EQ(filter_copy_remove, filter);
}

TEST(ClusterBarrier, SingleMatchingRuleWithBarrierItems) {
  auto processor = ClusterBarrier("n_local", "n_remote", ClusterLabels{{"wing", ClusterLabelValuePair{"wing_1", "wing_2"}}}, KeyList{"node_id"});

  auto filter = Filter{Constraint{"node_id", "n_n_n"}, Constraint{"wing", "wing_1"}, Constraint{"other1", "other_value"}};
  auto filter_copy_add = filter;
  auto filter_copy_remove = filter;

  ASSERT_TRUE(processor.process_added(&filter_copy_add));
  ASSERT_EQ(filter_copy_add, filter);

  ASSERT_TRUE(processor.process_removed(&filter_copy_remove));
  ASSERT_EQ(filter_copy_remove, filter);
}

TEST(ClusterBarrier, MultiMatchingRule) {
  auto processor = ClusterBarrier("n_local", "n_remote", ClusterLabels{{"wing", ClusterLabelValuePair{"wing_1", "wing_2"}}, {"floor", ClusterLabelValuePair{"floor_1", "floor_2"}}}, KeyList{"node_id"});

  auto filter = Filter{{"node_id", "n_n_n"}, Constraint{"other1", "other_value"}};
  auto filter_copy_add = filter;
  auto filter_copy_remove = filter;

  ASSERT_TRUE(processor.process_added(&filter_copy_add));
  ASSERT_EQ(filter_copy_add, filter);

  ASSERT_TRUE(processor.process_removed(&filter_copy_remove));
  ASSERT_EQ(filter_copy_remove, filter);
}

TEST(OptionalConstraintDrop, SingleMatchingRule) {
  auto processor = OptionalConstraintDrop("n_local", "n_remote", std::set<Constraint>{{"node_id", "n_local", ConstraintPredicates::Predicate::EQ, true}, {"bar", "foo"}});

  auto filter = Filter{{"foo", "bar"}, Constraint{"node_id", "n_local", ConstraintPredicates::Predicate::EQ, true}};
  auto filter_copy_add = Filter(filter);
  auto filter_copy_remove = Filter(filter);

  ASSERT_FALSE(processor.process_added(&filter_copy_add));
  ASSERT_NE(filter_copy_add, filter);

  ASSERT_FALSE(processor.process_removed(&filter_copy_remove));
  ASSERT_NE(filter_copy_remove, filter);
}


TEST(OptionalConstraintDrop, MultipleeMatchingRules) {
  auto processor = OptionalConstraintDrop("n_local", "n_remote", std::set<Constraint>{{"node_id", "n_local", ConstraintPredicates::Predicate::EQ, true}, {"bar", "foo"}});

  auto filter = Filter{{"foo", "bar"}, Constraint{"node_id", "n_local", ConstraintPredicates::Predicate::EQ, true}, Constraint{"asdf", "baz", ConstraintPredicates::Predicate::EQ, true}};
  auto filter_copy_add = Filter(filter);
  auto filter_copy_remove = Filter(filter);

  ASSERT_FALSE(processor.process_added(&filter_copy_add));
  ASSERT_NE(filter_copy_add, filter);

  ASSERT_FALSE(processor.process_removed(&filter_copy_remove));
  ASSERT_NE(filter_copy_remove, filter);
}

TEST(OptionalConstraintDrop, IgnoreDifferentRule) {
  auto processor = OptionalConstraintDrop("n_local", "n_remote", std::set<Constraint>{{"node_id", "n_local", ConstraintPredicates::Predicate::EQ, true}, {"bar", "foo"}});

  auto filter = Filter{{"foo", "bar"}, Constraint{"bar", "n_local", ConstraintPredicates::Predicate::EQ, true}};
  auto filter_copy_add = Filter(filter);
  auto filter_copy_remove = Filter(filter);

  ASSERT_FALSE(processor.process_added(&filter_copy_add));
  ASSERT_EQ(filter_copy_add, filter);

  ASSERT_FALSE(processor.process_removed(&filter_copy_remove));
  ASSERT_EQ(filter_copy_remove, filter);
}

};
