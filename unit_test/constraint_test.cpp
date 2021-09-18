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

}