#include <gtest/gtest.h>

#include <iostream>

#include "pubsub/constraint_index.hpp"
#include "pubsub/router.hpp"
#include "pubsub/allocator_configuration.hpp"
#include "support/tracking_allocator.hpp"

namespace uActor::Test {

  template <typename U>
  using Allocator = PubSub::RoutingAllocatorConfiguration::Allocator<U>;

  template <typename U>
  constexpr static auto make_allocator =
      PubSub::RoutingAllocatorConfiguration::make_allocator<U>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

template<typename A, typename B>
void test_different_values(A operand1, B operand2) {


  auto constraint1 = PubSub::Constraint{"foo", operand1};
  auto constraint2 = PubSub::Constraint{"foo", operand2};

  auto idx = PubSub::ConstraintIndex<Allocator>(make_allocator<char>());

  auto rec = PubSub::Router::get_instance().new_subscriber();
  
  auto sub = PubSub::Subscription<Allocator>{1, &*rec.receiver, 1, make_allocator<char>()};
  auto sub2 = PubSub::Subscription<Allocator>{2, &*rec.receiver, 1, make_allocator<char>()};

  auto insert_res = idx.insert(constraint1, &sub, false);
  ASSERT_TRUE(insert_res);

  PubSub::Counter<Allocator> c1; 
  idx.check(operand1, &c1);
  ASSERT_EQ(c1.size(), 1);

  PubSub::Counter<Allocator> c1b; 
  idx.check(operand2, &c1b);
  ASSERT_EQ(c1b.size(), 0);

  auto insert_res2 = idx.insert(constraint2, &sub2, true);
  ASSERT_TRUE(insert_res);

  ASSERT_NE(insert_res, insert_res2);

  PubSub::Counter<Allocator> c2; 
  idx.check(operand1, &c2);
  ASSERT_EQ(c2.size(), 1);
  ASSERT_TRUE(c2.find(&sub) != c2.end());

  PubSub::Counter<Allocator> c2b; 
  idx.check(operand2, &c2b);
  ASSERT_EQ(c2b.size(), 1);
  ASSERT_TRUE(c2b.find(&sub2) != c2.end());

  auto remove_result = idx.remove(constraint2, &sub2);
  ASSERT_FALSE(remove_result);

  PubSub::Counter<Allocator> c3; 
  idx.check(operand1, &c3);
  ASSERT_EQ(c3.size(), 1);
  ASSERT_TRUE(c3.find(&sub) != c3.end());

  PubSub::Counter<Allocator> c3b; 
  idx.check(operand2, &c3b);
  ASSERT_EQ(c3b.size(), 0);

  auto remove_result2 = idx.remove(constraint1, &sub);
  ASSERT_TRUE(remove_result2);

  PubSub::Counter<Allocator> c4; 
  idx.check(operand1, &c4);
  ASSERT_EQ(c4.size(), 0);

  PubSub::Counter<Allocator> c4b; 
  idx.check(operand2, &c4b);
  ASSERT_EQ(c4b.size(), 0); 
}

TEST(ConstraintIndex, two_strings_different_value) {
  test_different_values(std::string_view("a"), std::string_view("b"));
}

TEST(ConstraintIndex, two_strings_different_value_reversed) {
  test_different_values(std::string_view("b"), std::string_view("a")); 
}

TEST(ConstraintIndex, two_ints_different_value) {
  test_different_values(1, 2); 
}

TEST(ConstraintIndex, two_floats_different_value) {
  test_different_values(1.0f, 2.0f); 
}

TEST(ConstraintIndex, int_float) {
  test_different_values(1, 2.0f); 
}

TEST(ConstraintIndex, string_float) {
  test_different_values(std::string_view("a"), 2.0f); 
}


TEST(ConstraintIndex, two_strings_same_value) {
  auto constraint1 = PubSub::Constraint{"foo", "b"};
  auto constraint2 = PubSub::Constraint{"foo", "b"};

  auto idx = PubSub::ConstraintIndex<Allocator>(make_allocator<char>());

  auto rec = PubSub::Receiver(&PubSub::Router::get_instance());
  
  auto sub = PubSub::Subscription<Allocator>{ 1, &rec, 1, make_allocator<char>()};
  auto sub2 = PubSub::Subscription<Allocator>{ 2, &rec, 1, make_allocator<char>()};

  auto insert_res = idx.insert(constraint1, &sub, false);
  ASSERT_TRUE(insert_res);

  PubSub::Counter<Allocator> c; 
  idx.check(std::string_view("b"), &c);
  ASSERT_EQ(c.size(), 1);

  auto insert_res2 = idx.insert(constraint2, &sub2, true);
  ASSERT_TRUE(insert_res);

  ASSERT_EQ(insert_res, insert_res2);

  PubSub::Counter<Allocator> c2; 
  idx.check(std::string_view("b"), &c2);
  ASSERT_EQ(c2.size(), 2);
  ASSERT_TRUE(c2.find(&sub) != c2.end());
  ASSERT_TRUE(c2.find(&sub2) != c2.end());

  auto remove_result = idx.remove(constraint2, &sub2);
  ASSERT_FALSE(remove_result);

  PubSub::Counter<Allocator> c3; 
  idx.check(std::string_view("b"), &c3);
  ASSERT_EQ(c3.size(), 1);

  auto remove_result2 = idx.remove(constraint1, &sub);
  ASSERT_TRUE(remove_result2);  
}

} // namespace uActor::Test

template void uActor::Test::test_different_values(std::string_view, std::string_view);
template void uActor::Test::test_different_values(int, int);
template void uActor::Test::test_different_values(float, float);
template void uActor::Test::test_different_values(int, float);
template void uActor::Test::test_different_values(std::string_view, float);
template uActor::Support::TrackingAllocator<char> uActor::PubSub::RoutingAllocatorConfiguration::make_allocator<char>();