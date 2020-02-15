#include <arpa/inet.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "pubsub/router.hpp"
#include "remote_connection.hpp"

struct FakeForwarder : public ForwarderSubscriptionAPI {
  uint32_t add_subscription(uint32_t local_id, uActor::PubSub::Filter&& filter,
                            std::string_view node_id) {
    return 0;
  }
  void remove_subscription(uint32_t local_id, uint32_t sub_id,
                           std::string_view node_id) {}
  bool write(int sock, int len, const char* message) { return false; }

  static int next_sequence_number() {
    static int sequence_number{0};
    return sequence_number++;
  }
};

TEST(FORWARDING, data_handling_base) {
  auto subscription_handle =
      uActor::PubSub::Router::get_instance().new_subscriber();

  uActor::PubSub::Filter primary_filter{
      uActor::PubSub::Constraint(std::string("foo"), "bar")};
  subscription_handle.subscribe(primary_filter);
  FakeForwarder f;
  RemoteConnection connection{0, 0, &f};

  uActor::PubSub::Publication sub_update{"sender_node", "sender_type",
                                         "sender_id"};
  sub_update.set_attr("type", "subscription_update");
  sub_update.set_attr("subscription_node_id", "node_1");
  sub_update.set_attr("serialized_subscriptions",
                      uActor::PubSub::Filter{}.serialize() + "&");
  sub_update.set_attr("_internal_epoch", 0);
  sub_update.set_attr("_internal_sequence_number",
                      FakeForwarder::next_sequence_number());

  std::string serialized_update = sub_update.to_msg_pack();

  uint32_t size_update = htonl(serialized_update.size());
  connection.process_data(4, reinterpret_cast<char*>(&size_update));
  connection.process_data(serialized_update.size(), serialized_update.data());

  uActor::PubSub::Publication p{"sender_node", "sender_type", "sender_id"};
  p.set_attr("foo", "bar");
  p.set_attr("_internal_sequence_number",
             FakeForwarder::next_sequence_number());
  p.set_attr("_internal_epoch", 0);

  std::string serialized = p.to_msg_pack();

  uint32_t size = htonl(serialized.size());
  connection.process_data(4, reinterpret_cast<char*>(&size));
  connection.process_data(serialized.size(), serialized.data());

  auto result = subscription_handle.receive(0);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->publication.has_attr("_internal_forwarded_by"));
  ASSERT_STREQ(std::get<std::string_view>(
                   result->publication.get_attr("_internal_forwarded_by"))
                   .data(),
               "node_1");
}

TEST(FORWARDING, data_handling_split_data) {
  auto subscription_handle =
      uActor::PubSub::Router::get_instance().new_subscriber();

  uActor::PubSub::Filter primary_filter{
      uActor::PubSub::Constraint(std::string("foo"), "bar")};
  subscription_handle.subscribe(primary_filter);

  FakeForwarder f;
  RemoteConnection connection{0, 0, &f};

  uActor::PubSub::Publication p{"sender_node", "sender_type", "sender_id"};
  p.set_attr("foo", "bar");
  p.set_attr("_internal_sequence_number",
             FakeForwarder::next_sequence_number());
  p.set_attr("_internal_epoch", 0);

  std::string serialized = p.to_msg_pack();

  uint32_t size = htonl(serialized.size());
  connection.process_data(4, reinterpret_cast<char*>(&size));
  for (int i = 0; i < serialized.size(); i += 10) {
    connection.process_data(std::min(serialized.size() - i, 10ul),
                            serialized.data() + i);
  }

  ASSERT_TRUE(subscription_handle.receive(0));
}

TEST(FORWARDING, data_handling_split_message_size) {
  auto subscription_handle =
      uActor::PubSub::Router::get_instance().new_subscriber();

  uActor::PubSub::Filter primary_filter{
      uActor::PubSub::Constraint(std::string("foo"), "bar")};
  subscription_handle.subscribe(primary_filter);

  FakeForwarder f;
  RemoteConnection connection{0, 0, &f};

  uActor::PubSub::Publication p{"sender_node", "sender_type", "sender_id"};
  p.set_attr("foo", "bar");
  p.set_attr("_internal_sequence_number",
             FakeForwarder::next_sequence_number());
  p.set_attr("_internal_epoch", 0);

  std::string serialized = p.to_msg_pack();

  uint32_t size = htonl(serialized.size());
  connection.process_data(2, reinterpret_cast<char*>(&size));
  connection.process_data(1, reinterpret_cast<char*>(&size) + 2);
  connection.process_data(1, reinterpret_cast<char*>(&size) + 3);
  connection.process_data(serialized.size(), serialized.data());

  ASSERT_TRUE(subscription_handle.receive(0));
}

TEST(FORWARDING, mixed_data) {
  auto subscription_handle =
      uActor::PubSub::Router::get_instance().new_subscriber();

  uActor::PubSub::Filter primary_filter{
      uActor::PubSub::Constraint(std::string("foo"), "bar")};
  subscription_handle.subscribe(primary_filter);

  FakeForwarder f;
  RemoteConnection connection{0, 0, &f};

  uActor::PubSub::Publication p{"sender_node", "sender_type", "sender_id"};
  p.set_attr("foo", "bar");
  p.set_attr("_internal_sequence_number",
             FakeForwarder::next_sequence_number());
  p.set_attr("_internal_epoch", 0);

  std::string serialized = p.to_msg_pack();

  uint32_t size = htonl(serialized.size());

  std::vector<char> dummy = std::vector<char>(serialized.size() + 4);
  std::memcpy(dummy.data(), static_cast<void*>(&size), 4);
  std::memcpy(dummy.data() + 4, static_cast<void*>(serialized.data()),
              serialized.size());
  connection.process_data(dummy.size(), dummy.data());

  ASSERT_TRUE(subscription_handle.receive(0));
}
