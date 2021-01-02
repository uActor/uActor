#pragma once

#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <utility>

#include "executor_api.hpp"
#include "pubsub/filter.hpp"
#include "pubsub/publication.hpp"
#include "pubsub/router.hpp"

namespace uActor::ActorRuntime {

class ManagedActor {
 public:
  struct ReceiveResult {
    ReceiveResult(bool exit, uint32_t next_timeout)
        : exit(exit), next_timeout(next_timeout) {}
    bool exit;
    uint32_t next_timeout;
  };

  ManagedActor(const ManagedActor&) = delete;

  ManagedActor(ExecutorApi* api, uint32_t unique_id, std::string_view node_id,
               std::string_view actor_type, std::string_view actor_version,
               std::string_view instance_id);

  ~ManagedActor() {
    for (uint32_t sub_id : subscriptions) {
      api->remove_subscription(_id, sub_id);
    }
  }

  virtual bool receive(PubSub::Publication&& p) = 0;

  virtual std::string actor_runtime_type() = 0;

  std::pair<bool, uint32_t> early_initialize();

  bool late_initialize(std::string&& code);

  bool hibernate() {
    if (hibernate_internal()) {
      _initialized = false;
      return true;
    }
    return false;
  }

  bool wakeup() {
    if (wakeup_internal()) {
      _initialized = true;
      return true;
    }
    return false;
  }

  ReceiveResult receive_next_internal();

  bool enqueue(PubSub::Publication&& message);

  void trigger_timeout();

  [[nodiscard]] uint32_t id() const { return _id; }

  // Code is returned as part of a publication
  void trigger_code_fetch();

  [[nodiscard]] const char* node_id() const { return _node_id.c_str(); }

  [[nodiscard]] const char* actor_type() const { return _actor_type.c_str(); }

  [[nodiscard]] const char* instance_id() const { return _instance_id.c_str(); }

  [[nodiscard]] const char* actor_version() const {
    return _actor_version.c_str();
  }

  [[nodiscard]] bool initialized() const { return _initialized; }

 protected:
  virtual bool early_internal_initialize() = 0;
  virtual bool late_internal_initialize(std::string&& code) = 0;
  virtual bool hibernate_internal() = 0;
  virtual bool wakeup_internal() = 0;

  uint32_t subscribe(PubSub::Filter&& f);
  void unsubscribe(uint32_t sub_id);

  void publish(PubSub::Publication&& p);
  void delayed_publish(PubSub::Publication&& p, uint32_t delay);

  void deffered_block_for(PubSub::Filter&& filter, uint32_t timeout);

 private:
  uint32_t _id;

  bool waiting = false;
  PubSub::Filter pattern;
  uint32_t _timeout = UINT32_MAX;

  std::string _node_id;
  std::string _actor_type;
  std::string _actor_version;
  std::string _instance_id;

  std::deque<PubSub::Publication> message_queue;
  std::set<uint32_t> subscriptions;
  ExecutorApi* api;

  bool _initialized = false;

  uint32_t code_fetch_retries = 0;
  bool waiting_for_code = false;

  void publish_exit_message(std::string exit_reason);
  void publish_creation_message();
  void add_default_subscription();
};

}  //  namespace uActor::ActorRuntime
