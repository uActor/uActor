#include <gtest/gtest.h>

#include <pubsub/constraint.hpp>

namespace uActor::Test {

TEST(Constraint, prefix_match) {
    auto c = PubSub::Constraint("key", "val", PubSub::ConstraintPredicates::Predicate::PF);
    ASSERT_TRUE(c("value"));
}


TEST(Constraint, prefix_partial_match) {
    auto c = PubSub::Constraint("key", "val", PubSub::ConstraintPredicates::Predicate::PF);
    ASSERT_FALSE(c("vanotrigh"));
}

TEST(Constraint, prefix_partial_match2) {
    auto c = PubSub::Constraint("key", "val", PubSub::ConstraintPredicates::Predicate::PF);
    ASSERT_FALSE(c("alue"));
}

TEST(Constraint, prefix_no_match) {
        auto c = PubSub::Constraint("key", "val", PubSub::ConstraintPredicates::Predicate::PF);
    ASSERT_FALSE(c("foobar"));
}

TEST(Constraint, suffix_match) {
    auto c = PubSub::Constraint("key", "lue", PubSub::ConstraintPredicates::Predicate::SF);
    ASSERT_TRUE(c("value"));
}


TEST(Constraint, suffix_partial_match) {
    auto c = PubSub::Constraint("key", "val", PubSub::ConstraintPredicates::Predicate::SF);
    ASSERT_FALSE(c("vadue"));
}

TEST(Constraint, suffix_partial_match2) {
    auto c = PubSub::Constraint("key", "val", PubSub::ConstraintPredicates::Predicate::SF);
    ASSERT_FALSE(c("valu"));
}

TEST(Constraint, suffix_no_match) {
    auto c = PubSub::Constraint("key", "val", PubSub::ConstraintPredicates::Predicate::SF);
    ASSERT_FALSE(c("foobar"));
}

TEST(Constraint, contains_yes) {
    auto c = PubSub::Constraint("key", "bcd", PubSub::ConstraintPredicates::Predicate::CT);
    ASSERT_TRUE(c("abcde"));
}

TEST(Constraint, contains_no) {
    auto c = PubSub::Constraint("key", "bcd", PubSub::ConstraintPredicates::Predicate::CT);
    ASSERT_FALSE(c("abade"));
}

TEST(Constraint, nested_constraint_base_valid) {
    auto c = PubSub::Constraint("key", std::vector<PubSub::Constraint>{PubSub::Constraint("nested_key", "nested_value")}, PubSub::ConstraintPredicates::Predicate::MAP_ALL);
    auto input = std::make_shared<PubSub::Publication::Map>();
    input->set_attr("nested_key", "nested_value");
    ASSERT_TRUE(c(*input));
}

TEST(Constraint, nested_constraint_base_invalid) {
    auto c = PubSub::Constraint("key", std::vector<PubSub::Constraint>{PubSub::Constraint("nested_key", "nested_value")}, PubSub::ConstraintPredicates::Predicate::MAP_ALL);
    auto input = std::make_shared<PubSub::Publication::Map>();
    input->set_attr("nested_key", "not_the_nested_value");
    ASSERT_FALSE(c(*input));
}

TEST(Constraint, nested_constraint_base_not_contained) {
    auto c = PubSub::Constraint("key", std::vector<PubSub::Constraint>{PubSub::Constraint("nested_key", "nested_value")}, PubSub::ConstraintPredicates::Predicate::MAP_ALL);
    auto input = std::make_shared<PubSub::Publication::Map>();
    input->set_attr("not_the_nested_key", "not_the_nested_value");
    ASSERT_FALSE(c(*input));
}

TEST(Constraint, nested_constraint_base_empty) {
    auto c = PubSub::Constraint("key", std::vector<PubSub::Constraint>{PubSub::Constraint("nested_key", "nested_value")}, PubSub::ConstraintPredicates::Predicate::MAP_ALL);
    auto input = std::make_shared<PubSub::Publication::Map>();
    ASSERT_FALSE(c(*input));
}

TEST(Constraint, nested_constraint_multiple_valid) {
    auto c = PubSub::Constraint("key", std::vector<PubSub::Constraint>{PubSub::Constraint("nested_key", "nested_value"), PubSub::Constraint("nested_key2", static_cast<int32_t>(2))}, PubSub::ConstraintPredicates::Predicate::MAP_ALL);
    auto input = std::make_shared<PubSub::Publication::Map>();
    input->set_attr("nested_key", "nested_value");
    input->set_attr("nested_key2", static_cast<int32_t>(2));
    ASSERT_TRUE(c(*input));
}

TEST(Constraint, nested_constraint_base_one_missing) {
    auto c = PubSub::Constraint("key", std::vector<PubSub::Constraint>{PubSub::Constraint("nested_key", "nested_value"), PubSub::Constraint("nested_key2", static_cast<int32_t>(2))}, PubSub::ConstraintPredicates::Predicate::MAP_ALL);
    auto input = std::make_shared<PubSub::Publication::Map>();
    input->set_attr("nested_key", "nested_value");
    ASSERT_FALSE(c(*input));
}

TEST(Constraint, nested_constraint_base_one_invalid) {
    auto c = PubSub::Constraint("key", std::vector<PubSub::Constraint>{PubSub::Constraint("nested_key", "nested_value"), PubSub::Constraint("nested_key2", static_cast<int32_t>(2))}, PubSub::ConstraintPredicates::Predicate::MAP_ALL);
    auto input = std::make_shared<PubSub::Publication::Map>();
    input->set_attr("nested_key", "nested_value");
    input->set_attr("nested_key2", static_cast<int32_t>(3)); 
    ASSERT_FALSE(c(*input));
}

}