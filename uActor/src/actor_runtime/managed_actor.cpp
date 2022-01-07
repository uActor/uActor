#include "actor_runtime/managed_actor.hpp"

#if ESP_IDF
#include <esp_attr.h>
#include <sdkconfig.h>
#endif

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

#include "support/logger.hpp"

#define ACTOR_QUEUE_SOFTLIMIT 100

namespace uActor::ActorRuntime {

#if CONFIG_UACTOR_ENABLE_TELEMETRY
std::atomic<int> ManagedActor::total_queue_size{0};
#endif

#if CONFIG_UACTOR_OPTIMIZATIONS_IRAM
IRAM_ATTR
#endif
ManagedActor::ReceiveResult ManagedActor::receive_next_internal() {
  waiting = false;
  _timeout = UINT32_MAX;
  pattern.clear();

  auto next_message = std::move(message_queue.front());
  message_queue.pop_front();
#if CONFIG_UACTOR_ENABLE_TELEMETRY
  total_queue_size--;
#endif
  // message_queue.shrink_to_fit();
  // Exit message is processed to
  // allow for any necessary cleanup
  bool is_exit = next_message.get_str_attr("type") == "exit";
  auto ret = RuntimeReturnValue::NONE;
  if (next_message.get_str_attr("type") == "code_fetch_response" &&
      std::string_view("code_store") != actor_type()) {
    if (!_initialized) {
      waiting_for_code = false;
      if (next_message.get_str_attr("fetch_actor_type") == actor_type()) {
        ret = late_initialize(
            std::string(*next_message.get_str_attr("fetch_actor_code")));
      } else {
        Support::Logger::fatal(
            "MANAGED-ACTOR",
            "CODE FETCH: Received wrong code package. Received: %s Expected: "
            "%s\n",
            next_message.get_str_attr("fetch_actor_type")->data(),
            actor_type());
      }
    } else {
      Support::Logger::info("MANAGED-ACTOR", "CODE FETCH: Delayed Code");
    }
  } else if (next_message.get_str_attr("type") == "timeout" &&
             waiting_for_code) {
    if (code_fetch_retries < 20) {
      Support::Logger::debug("MANAGED-ACTOR", "CODE FETCH: RETRY");
      trigger_code_fetch();
      code_fetch_retries++;
      return ManagedActor::ReceiveResult(false, _timeout);
    } else {
      Support::Logger::warning("MANAGED-ACTOR", "CODE FETCH: FAILED");
      ret = RuntimeReturnValue::INITIALIZATION_ERROR;
    }
  } else {
    if (!initialized()) {
      ret = wakeup();
      if (ret == RuntimeReturnValue::NOT_READY) {
        message_queue.emplace_front(std::move(next_message));
      }
    }
    if (ret == RuntimeReturnValue::NONE || ret == RuntimeReturnValue::OK) {
      reply_context.set(next_message);
      ret = receive(std::move(next_message));
      reply_context.reset();
      if (ret == RuntimeReturnValue::NOT_READY) {
        Support::Logger::error("MANAGED-ACTOR",
                               "Receive called on inactive actor");
        ret = RuntimeReturnValue::RUNTIME_ERROR;
      }
#if CONFIG_UACTOR_HIBERNATION
      hibernate();
#endif
    }
  }
  if (ret == RuntimeReturnValue::INITIALIZATION_ERROR ||
      ret == RuntimeReturnValue::RUNTIME_ERROR || is_exit) {
    if (ret == RuntimeReturnValue::OK) {
      publish_exit_message("clean_exit");
    } else if (ret == RuntimeReturnValue::INITIALIZATION_ERROR) {
      publish_exit_message("initialization_failure");
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
  } else if (!message_queue.empty()) {
    return ManagedActor::ReceiveResult(false, 0);
  }
  return ManagedActor::ReceiveResult(false, _timeout);
}

bool ManagedActor::enqueue(PubSub::Publication&& message) {
#if CONFIG_UACTOR_ENABLE_TELEMETRY
  total_queue_size++;
#endif
  if (message.get_str_attr("type") == "exit") {
    message_queue.push_front(std::move(message));
    return true;
  }
  if (waiting) {
    if (pattern.matches(message)) {
      // waiting = false;
      message_queue.push_front(std::move(message));
      return true;
    } else {
      message_queue.push_back(std::move(message));
      return false;
    }
  } else {
    message_queue.push_back(std::move(message));
    return true;
  }
}

void ManagedActor::trigger_timeout(bool user_defined,
                                   std::string_view wakeup_id) {
  Support::Logger::trace("MANAGED-ACTOR", "TRIGGER-TIMEOUT", "called");
  auto p = PubSub::Publication(_node_id.c_str(), _actor_type.c_str(),
                               _instance_id.c_str());
  if (!user_defined) {
    p.set_attr("_internal_timeout", "_timeout");
    p.set_attr("type", "timeout");
  } else {
    p.set_attr("type", "wakeup");
    p.set_attr("wakeup_id", wakeup_id);
  }
  message_queue.emplace_front(std::move(p));
#if CONFIG_UACTOR_ENABLE_TELEMETRY
  total_queue_size++;
#endif
}

std::pair<ManagedActor::RuntimeReturnValue, uint32_t>
ManagedActor::early_initialize() {
  // We need to wrap the runtime-specific initialization.
  auto ret = early_internal_initialize();
  _initialized = ret == RuntimeReturnValue::OK;
  return std::make_pair(ret, _timeout);
}

ManagedActor::RuntimeReturnValue ManagedActor::late_initialize(
    std::string&& code) {
  // We need to wrap the runtime-specific initialization.
  Support::Logger::trace("MANAGED-ACTOR", "Late init called");
  auto ret = late_internal_initialize(std::move(code));
  _initialized = ret == RuntimeReturnValue::OK;
  if (_initialized) {
    Support::Logger::trace("MANAGED-ACTOR", "Late init success");
    return RuntimeReturnValue::OK;
  } else {
    publish_exit_message("initialization_failure");
    Support::Logger::trace("MANAGED-ACTOR", "Late init failure");
    return RuntimeReturnValue::INITIALIZATION_ERROR;
  }
}

void ManagedActor::trigger_code_fetch() {
  waiting_for_code = true;
  Support::Logger::info("MANAGED-ACTOR", "Trigger code fetch\n");
  deffered_block_for(
      PubSub::Filter{PubSub::Constraint("type", "code_fetch_response")}, 1000);
  PubSub::Publication fetch_code(node_id(), actor_type(), instance_id());
  // fetch_code.set_attr("node_id", node_id());
  fetch_code.set_attr("type", "code_fetch_request");
  fetch_code.set_attr("fetch_actor_type", std::string_view(_actor_type));
  fetch_code.set_attr("fetch_actor_version", std::string_view(_actor_version));
  fetch_code.set_attr("fetch_actor_runtime_type", actor_runtime_type());
  publish(std::move(fetch_code));
}

uint32_t ManagedActor::subscribe(PubSub::Filter&& f, uint8_t priority) {
  uint32_t sub_id = api->add_subscription(_id, std::move(f), priority);
  subscriptions.insert(sub_id);
  return sub_id;
}

void ManagedActor::unsubscribe(uint32_t sub_id) {
  subscriptions.erase(sub_id);
  api->remove_subscription(_id, sub_id);
}

void ManagedActor::publish(PubSub::Publication&& p) {
  p.set_attr("publisher_node_id", _node_id);
  p.set_attr("publisher_instance_id", _instance_id);
  p.set_attr("publisher_actor_type", _actor_type);
  PubSub::Router::get_instance().publish(std::move(p));
}

void ManagedActor::republish(PubSub::Publication&& p) {
  p.set_attr("processor_node_id", _node_id);
  p.set_attr("processor_instance_id", _instance_id);
  p.set_attr("processor_actor_type", _actor_type);
  PubSub::Router::get_instance().publish(std::move(p));
}

void ManagedActor::delayed_publish(PubSub::Publication&& p, uint32_t delay) {
  api->delayed_publish(std::move(p), delay);
}

void ManagedActor::enqueue_wakeup(uint32_t delay, std::string_view wakeup_id) {
  api->enqueue_wakeup(_id, delay, wakeup_id);
}

uint32_t ManagedActor::queue_size() { return message_queue.size(); }

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
      _id, PubSub::Filter{PubSub::Constraint(std::string("node_id"),
                                             std::string(_node_id)),
                          PubSub::Constraint(std::string("actor_type"),
                                             std::string(_actor_type)),
                          PubSub::Constraint(std::string("instance_id"),
                                             std::string(_instance_id))});
  subscriptions.insert(sub_id);
}

}  // namespace uActor::ActorRuntime
