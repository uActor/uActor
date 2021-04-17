#include <arpa/inet.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "pubsub/router.hpp"
#include "remote/remote_connection.hpp"

namespace uActor::Test {

struct FakeForwarder : public Remote::ForwarderSubscriptionAPI {

  virtual uint32_t add_remote_subscription(uint32_t local_id,
                                           PubSub::Filter&& filter,
                                           std::string node_id) {
    return 0;
  };
  virtual void remove_remote_subscription(uint32_t local_id, uint32_t sub_id,
                                          std::string node_id) {};

  virtual uint32_t add_local_subscription(uint32_t local_id,
                                          PubSub::Filter&& filter) {
    return 0;
  };
  virtual void remove_local_subscription(uint32_t local_id,
                                         uint32_t sub_id) {};

  bool write(int sock, int len, const char* message) { return false; }

  static int next_sequence_number() {
    static int sequence_number{0};
    return sequence_number++;
  }
};

TEST(FORWARDING, data_handling_base) {
  auto subscription_handle = PubSub::Router::get_instance().new_subscriber();

  PubSub::Filter primary_filter{PubSub::Constraint(std::string("foo"), "bar")};
  subscription_handle.subscribe(primary_filter);
  FakeForwarder f;
  Remote::RemoteConnection connection{0, 0, "127.0.0.1", 1234, Remote::ConnectionRole::CLIENT, &f};

  PubSub::Publication sub_update{"sender_node", "sender_type", "sender_id"};
  sub_update.set_attr("type", "subscription_update");
  sub_update.set_attr("subscription_node_id", "node_1");
  sub_update.set_attr("_internal_epoch", 0);
  sub_update.set_attr("_internal_sequence_number",
                      FakeForwarder::next_sequence_number());
  auto serialized_update = sub_update.to_msg_pack();
  uint32_t size_update = serialized_update->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized_update->data()) =
      htonl(size_update);
  connection.process_data(serialized_update->size(), serialized_update->data());

  PubSub::Publication sub_added{"sender_node", "sender_type", "sender_id"};
  sub_added.set_attr("type", "subscriptions_added");
  sub_added.set_attr("serialized_subscriptions",
                    PubSub::Filter{}.serialize() + "&");
  sub_added.set_attr("_internal_epoch", 0);
  sub_added.set_attr("_internal_sequence_number",
                    FakeForwarder::next_sequence_number());
  auto serialized_added = sub_added.to_msg_pack();
  uint32_t size_added = serialized_added->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized_added->data()) =
        htonl(size_added);
  connection.process_data(serialized_added->size(), serialized_added->data());

  PubSub::Publication p{"sender_node", "sender_type", "sender_id"};
  p.set_attr("foo", "bar");
  p.set_attr("_internal_sequence_number",
             FakeForwarder::next_sequence_number());
  p.set_attr("_internal_epoch", 0);

  auto serialized = p.to_msg_pack();
  uint32_t size_msg = serialized->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized->data()) =
      htonl(size_msg);

  connection.process_data(4, serialized->data());
  connection.process_data(serialized->size() - 4, serialized->data() + 4);

  auto result = subscription_handle.receive(0);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->publication.has_attr("_internal_forwarded_by"));
  ASSERT_STREQ(std::get<std::string_view>(
                   result->publication.get_attr("_internal_forwarded_by"))
                   .data(),
               "node_1");
}

TEST(FORWARDING, data_handling_split_data) {
  auto subscription_handle = PubSub::Router::get_instance().new_subscriber();

  PubSub::Filter primary_filter{PubSub::Constraint(std::string("foo"), "bar")};
  subscription_handle.subscribe(primary_filter);

  FakeForwarder f;
  Remote::RemoteConnection connection{0, 0, "127.0.0.1", 1234, Remote::ConnectionRole::CLIENT, &f};

  PubSub::Publication sub_update{"sender_node", "sender_type", "sender_id"};
  sub_update.set_attr("type", "subscription_update");
  sub_update.set_attr("subscription_node_id", "node_1");
  sub_update.set_attr("_internal_epoch", 0);
  sub_update.set_attr("_internal_sequence_number",
                      FakeForwarder::next_sequence_number());
  auto serialized_update = sub_update.to_msg_pack();
  uint32_t size_update = serialized_update->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized_update->data()) =
      htonl(size_update);
  connection.process_data(serialized_update->size(), serialized_update->data());

  PubSub::Publication sub_added{"sender_node", "sender_type", "sender_id"};
  sub_added.set_attr("type", "subscriptions_added");
  sub_added.set_attr("serialized_subscriptions",
                     PubSub::Filter{}.serialize() + "&");
  sub_added.set_attr("_internal_epoch", 0);
  sub_added.set_attr("_internal_sequence_number",
                     FakeForwarder::next_sequence_number());

  auto serialized_added = sub_added.to_msg_pack();
  uint32_t size_added = serialized_added->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized_added->data()) =
      htonl(size_added);
  connection.process_data(serialized_added->size(), serialized_added->data());

  PubSub::Publication p{"sender_node", "sender_type", "sender_id"};
  p.set_attr("foo", "bar");
  p.set_attr("_internal_sequence_number",
             FakeForwarder::next_sequence_number());
  p.set_attr("_internal_epoch", 0);

  auto serialized = p.to_msg_pack();
  uint32_t size_msg = serialized->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized->data()) =
      htonl(size_msg);

  connection.process_data(4, serialized->data());
  for (int i = 4; i < serialized->size(); i += 10) {
    connection.process_data(std::min(serialized->size() - i, 10ul),
                            serialized->data() + i);
  }

  ASSERT_TRUE(subscription_handle.receive(0));
}

TEST(FORWARDING, data_handling_split_message_size) {
  auto subscription_handle = PubSub::Router::get_instance().new_subscriber();

  PubSub::Filter primary_filter{PubSub::Constraint(std::string("foo"), "bar")};
  subscription_handle.subscribe(primary_filter);

  FakeForwarder f;
  Remote::RemoteConnection connection{0, 0, "127.0.0.1", 1234, Remote::ConnectionRole::CLIENT, &f};

  PubSub::Publication sub_update{"sender_node", "sender_type", "sender_id"};
  sub_update.set_attr("type", "subscription_update");
  sub_update.set_attr("subscription_node_id", "node_1");
  sub_update.set_attr("_internal_epoch", 0);
  sub_update.set_attr("_internal_sequence_number",
                      FakeForwarder::next_sequence_number());
  auto serialized_update = sub_update.to_msg_pack();
  uint32_t size_update = serialized_update->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized_update->data()) =
      htonl(size_update);
  connection.process_data(serialized_update->size(), serialized_update->data());

  PubSub::Publication sub_added{"sender_node", "sender_type", "sender_id"};
  sub_added.set_attr("type", "subscriptions_added");
  sub_added.set_attr("serialized_subscriptions",
                     PubSub::Filter{}.serialize() + "&");
  sub_added.set_attr("_internal_epoch", 0);
  sub_added.set_attr("_internal_sequence_number",
                     FakeForwarder::next_sequence_number());
  auto serialized_added = sub_added.to_msg_pack();
  uint32_t size_added = serialized_added->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized_added->data()) =
      htonl(size_added);
  connection.process_data(serialized_added->size(), serialized_added->data());

  PubSub::Publication p{"sender_node", "sender_type", "sender_id"};
  p.set_attr("foo", "bar");
  p.set_attr("_internal_sequence_number",
             FakeForwarder::next_sequence_number());
  p.set_attr("_internal_epoch", 0);

  auto serialized = p.to_msg_pack();
  uint32_t size_msg = serialized->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized->data()) =
      htonl(size_msg);

  connection.process_data(2, serialized->data());
  connection.process_data(1, serialized->data() + 2);
  connection.process_data(1, serialized->data() + 3);
  connection.process_data(serialized->size() - 4, serialized->data()+4);

  ASSERT_TRUE(subscription_handle.receive(0));
}

TEST(FORWARDING, mixed_data) {
  auto subscription_handle = PubSub::Router::get_instance().new_subscriber();

  PubSub::Filter primary_filter{PubSub::Constraint(std::string("foo"), "bar")};
  subscription_handle.subscribe(primary_filter);

  FakeForwarder f;
  Remote::RemoteConnection connection{0, 0, "127.0.0.1", 1234, Remote::ConnectionRole::CLIENT, &f};

  PubSub::Publication sub_update{"sender_node", "sender_type", "sender_id"};
  sub_update.set_attr("type", "subscription_update");
  sub_update.set_attr("subscription_node_id", "node_1");
  sub_update.set_attr("_internal_epoch", 0);
  sub_update.set_attr("_internal_sequence_number",
                      FakeForwarder::next_sequence_number());
  auto serialized_update = sub_update.to_msg_pack();
  uint32_t size_update = serialized_update->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized_update->data()) =
      htonl(size_update);
  connection.process_data(serialized_update->size(), serialized_update->data());

  PubSub::Publication sub_added{"sender_node", "sender_type", "sender_id"};
  sub_added.set_attr("type", "subscriptions_added");
  sub_added.set_attr("serialized_subscriptions",
                     PubSub::Filter{}.serialize() + "&");
  sub_added.set_attr("_internal_epoch", 0);
  sub_added.set_attr("_internal_sequence_number",
                     FakeForwarder::next_sequence_number());
  auto serialized_added = sub_added.to_msg_pack();
  uint32_t size_added = serialized_added->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized_added->data()) =
      htonl(size_added);
  connection.process_data(serialized_added->size(), serialized_added->data());

  PubSub::Publication p{"sender_node", "sender_type", "sender_id"};
  p.set_attr("foo", "bar");
  p.set_attr("_internal_sequence_number",
             FakeForwarder::next_sequence_number());
  p.set_attr("_internal_epoch", 0);

  auto serialized = p.to_msg_pack();
  uint32_t size_msg = serialized->size() - 4;
  *reinterpret_cast<uint32_t*>(serialized->data()) =
      htonl(size_msg);

  connection.process_data(serialized->size(), serialized->data());

  ASSERT_TRUE(subscription_handle.receive(0));
}

}  // namespace uActor::Test
