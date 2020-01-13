#include "managed_actor.hpp"

#define ACTOR_QUEUE_SOFTLIMIT 100

ManagedActor::ManagedActor(RuntimeApi* api, uint32_t unique_id,
                           const char* node_id, const char* actor_type,
                           const char* instance_id, const char* code)
    : _id(unique_id),
      _node_id(node_id),
      _actor_type(actor_type),
      _instance_id(instance_id),
      _code(code),
      api(api) {
  add_default_subscription();
  send_creation_message();
}

ManagedActor::ReceiveResult ManagedActor::receive_next_internal() {
  waiting = false;
  _timeout = UINT32_MAX;
  pattern.filter.clear();

  const auto& next_message = message_queue.front();
  bool success =
      this->receive(message_queue.front());  // Exit message is processed to
                                             // allow for any necessary cleanup
  if (!success ||
      (next_message.has_attr("type") &&
       std::get<std::string_view>(next_message.get_attr("type")) == "exit")) {
    if (success) {
      send_exit_message("clean_exit");
    } else {
      send_exit_message("runtime_failure");
    }
    return ManagedActor::ReceiveResult(true, 0);
  }
  message_queue.pop_front();
  if (waiting) {
    auto it = std::find_if(
        message_queue.begin(), message_queue.end(),
        [&](Publication& message) { return pattern.matches(message); });
    if (it != message_queue.end()) {
      Publication message = std::move(*it);
      message_queue.erase(it);
      message_queue.push_front(std::move(message));
      return ManagedActor::ReceiveResult(false, 0);
    }
  } else if (message_queue.size() > 0) {
    return ManagedActor::ReceiveResult(false, 0);
  }
  return ManagedActor::ReceiveResult(false, _timeout);
}

bool ManagedActor::enqueue(Publication&& pub) {
  if (message_queue.size() >= ACTOR_QUEUE_SOFTLIMIT) {
    printf("Warning: Actor queue size excedes configured limit.");
  }
  if (waiting) {
    if (pattern.matches(pub)) {
      message_queue.push_front(std::move(pub));
      return true;
    } else {
      message_queue.push_back(std::move(pub));
      return false;
    }
  } else {
    message_queue.push_back(std::move(pub));
    return true;
  }
}

bool ManagedActor::initialize() {
  // We need to wrap the runtime-specific initialization.
  _initialized = internal_initialize();
  if (_initialized) {
    return true;
  } else {
    send_exit_message("initialization_failure");
    return false;
  }
}

void ManagedActor::send_exit_message(std::string exit_reason) {
  Publication exit_message = Publication(_node_id, _actor_type, _instance_id);
  exit_message.set_attr("category", "actor_lifetime");
  exit_message.set_attr("type", "actor_exit");
  exit_message.set_attr("exit_reason", exit_reason);
  exit_message.set_attr("exit_node_id", _node_id);
  exit_message.set_attr("exit_actor_type", _actor_type);
  exit_message.set_attr("exit_instance_id", _instance_id);
  PubSub::Router::get_instance().publish(std::move(exit_message));
}

void ManagedActor::send_creation_message() {
  Publication create_message = Publication(_node_id, _actor_type, _instance_id);
  create_message.set_attr("category", "actor_lifetime");
  create_message.set_attr("type", "actor_creation");
  create_message.set_attr("creation_node_id", _node_id);
  create_message.set_attr("creation_actor_type", _actor_type);
  create_message.set_attr("creation_instance_id", _instance_id);
  PubSub::Router::get_instance().publish(std::move(create_message));
}

void ManagedActor::add_default_subscription() {
  uint32_t sub_id = api->add_subscription(
      _id, PubSub::Filter{
               PubSub::Constraint(std::string("node_id"), _node_id),
               PubSub::Constraint(std::string("actor_type"), _actor_type),
               PubSub::Constraint(std::string("?instance_id"), _instance_id)});
  subscriptions.insert(sub_id);
}
