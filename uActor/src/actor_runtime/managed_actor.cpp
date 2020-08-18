#include "actor_runtime/managed_actor.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

#define ACTOR_QUEUE_SOFTLIMIT 100

namespace uActor::ActorRuntime {

ManagedActor::ManagedActor(ExecutorApi* api, uint32_t unique_id,
                           const char* node_id, const char* actor_type,
                           const char* actor_version, const char* instance_id)
    : _id(unique_id),
      _node_id(node_id),
      _actor_type(actor_type),
      _actor_version(actor_version),
      _instance_id(instance_id),
      api(api) {
  add_default_subscription();
  publish_creation_message();
}

ManagedActor::ReceiveResult ManagedActor::receive_next_internal() {
  waiting = false;
  _timeout = UINT32_MAX;
  pattern.clear();

  auto next_message = std::move(message_queue.front());
  message_queue.pop_front();
  // Exit message is processed to
  // allow for any necessary cleanup
  bool is_exit = next_message.get_str_attr("type") == "exit";
  bool success = false;
  if (next_message.get_str_attr("type") == "fetch_actor_code_response") {
    success =
        late_initialize(std::string(*next_message.get_str_attr("actor_code")));
  } else {
    success = this->receive(std::move(next_message));
  }
  if (!success || is_exit) {
    if (success) {
      publish_exit_message("clean_exit");
    } else {
      publish_exit_message("runtime_failure");
    }
    return ManagedActor::ReceiveResult(true, 0);
  }
  if (waiting) {
    auto it = std::find_if(
        message_queue.begin(), message_queue.end(),
        [&](PubSub::Publication& message) { return pattern.matches(message); });
    if (it != message_queue.end()) {
      PubSub::Publication message = std::move(*it);
      message_queue.erase(it);
      message_queue.push_front(std::move(message));
      return ManagedActor::ReceiveResult(false, 0);
    }
  } else if (message_queue.size() > 0) {
    return ManagedActor::ReceiveResult(false, 0);
  }
  return ManagedActor::ReceiveResult(false, _timeout);
}

bool ManagedActor::enqueue(PubSub::Publication&& pub) {
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

void ManagedActor::trigger_timeout() {
  auto p = PubSub::Publication(_node_id.c_str(), _actor_type.c_str(),
                               _instance_id.c_str());
  p.set_attr("_internal_timeout", "_timeout");
  message_queue.emplace_front(std::move(p));
}

bool ManagedActor::early_initialize() {
  // We need to wrap the runtime-specific initialization.
  _initialized = early_internal_initialize();
  return _initialized;
}

bool ManagedActor::late_initialize(std::string&& code) {
  // We need to wrap the runtime-specific initialization.
  _initialized = late_internal_initialize(std::move(code));
  if (_initialized) {
    return true;
  } else {
    publish_exit_message("initialization_failure");
    return false;
  }
}

void ManagedActor::trigger_code_fetch() {
  PubSub::Publication fetch_code(node_id(), actor_type(), instance_id());
  fetch_code.set_attr("command", "fetch_actor_code");
  fetch_code.set_attr("actor_code_type", _actor_type);
  fetch_code.set_attr("actor_code_version", _actor_version);
  fetch_code.set_attr("actor_code_runtime_type", actor_runtime_type());
  delayed_publish(std::move(fetch_code), 2000);
}

uint32_t ManagedActor::subscribe(PubSub::Filter&& f) {
  uint32_t sub_id = api->add_subscription(_id, std::move(f));
  subscriptions.insert(sub_id);
  return sub_id;
}

void ManagedActor::unsubscribe(uint32_t sub_id) {
  subscriptions.erase(sub_id);
  api->remove_subscription(_id, sub_id);
}

void ManagedActor::publish(PubSub::Publication&& p) {
  PubSub::Router::get_instance().publish(std::move(p));
}

void ManagedActor::delayed_publish(PubSub::Publication&& p, uint32_t delay) {
  api->delayed_publish(std::move(p), delay);
}

void ManagedActor::deffered_block_for(PubSub::Filter&& filter,
                                      uint32_t timeout) {
  waiting = true;
  pattern = std::move(filter);
  _timeout = timeout;
}

void ManagedActor::publish_exit_message(std::string exit_reason) {
  auto exit_message = PubSub::Publication(_node_id, _actor_type, _instance_id);
  exit_message.set_attr("category", "actor_lifetime");
  exit_message.set_attr("type", "actor_exit");
  exit_message.set_attr("exit_reason", exit_reason);
  exit_message.set_attr("lifetime_node_id", _node_id);
  exit_message.set_attr("lifetime_actor_type", _actor_type);
  exit_message.set_attr("lifetime_instance_id", _instance_id);
  PubSub::Router::get_instance().publish(std::move(exit_message));
}

void ManagedActor::publish_creation_message() {
  auto create_message =
      PubSub::Publication(_node_id, _actor_type, _instance_id);
  create_message.set_attr("category", "actor_lifetime");
  create_message.set_attr("type", "actor_creation");
  create_message.set_attr("lifetime_node_id", _node_id);
  create_message.set_attr("lifetime_actor_type", _actor_type);
  create_message.set_attr("lifetime_instance_id", _instance_id);
  PubSub::Router::get_instance().publish(std::move(create_message));
}

void ManagedActor::add_default_subscription() {
  uint32_t sub_id = api->add_subscription(
      _id, PubSub::Filter{
               PubSub::Constraint(std::string("node_id"), _node_id),
               PubSub::Constraint(std::string("actor_type"), _actor_type),
               PubSub::Constraint(std::string("instance_id"), _instance_id)});
  subscriptions.insert(sub_id);
}

}  // namespace uActor::ActorRuntime
