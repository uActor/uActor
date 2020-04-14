#ifndef MAIN_INCLUDE_MANAGED_ACTOR_HPP_
#define MAIN_INCLUDE_MANAGED_ACTOR_HPP_

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>

#include "pubsub/router.hpp"
#include "executor_api.hpp"

class Pattern {
 public:
  Pattern() : filter() {}

  bool matches(const uActor::PubSub::Publication& publication) {
    return filter.matches(publication);
  }

  uActor::PubSub::Filter filter;
};

class ManagedActor {
 public:
  struct ReceiveResult {
    ReceiveResult(bool exit, uint32_t next_timeout)
        : exit(exit), next_timeout(next_timeout) {}
    bool exit;
    uint32_t next_timeout;
  };

  ManagedActor(const ManagedActor&) = delete;

  ManagedActor(ExecutorApi* api, uint32_t unique_id, const char* node_id,
               const char* actor_type, const char* instance_id,
               const char* code);

  ~ManagedActor() {
    for (uint32_t sub_id : subscriptions) {
      api->remove_subscription(_id, sub_id);
    }
  }

  virtual bool receive(const uActor::PubSub::Publication& p) = 0;

  bool initialize();

  ReceiveResult receive_next_internal();

  bool enqueue(uActor::PubSub::Publication&& message);

  void trigger_timeout() {
    auto p = uActor::PubSub::Publication(_node_id.c_str(), _actor_type.c_str(),
                                         _instance_id.c_str());
    p.set_attr("_internal_timeout", "_timeout");
    message_queue.emplace_front(std::move(p));
  }

  uint32_t id() { return _id; }

  const char* code() { return _code.c_str(); }

  const char* node_id() const { return _node_id.c_str(); }

  const char* actor_type() const { return _actor_type.c_str(); }

  const char* instance_id() const { return _instance_id.c_str(); }

  bool initialized() const { return _initialized; }

 protected:
  virtual bool internal_initialize() = 0;

  uint32_t subscribe(uActor::PubSub::Filter&& f) {
    uint32_t sub_id = api->add_subscription(_id, std::move(f));
    subscriptions.insert(sub_id);
    return sub_id;
  }

  void unsubscribe(uint32_t sub_id) {
    subscriptions.erase(sub_id);
    api->remove_subscription(_id, sub_id);
  }

  void publish(uActor::PubSub::Publication&& p) {
    uActor::PubSub::Router::get_instance().publish(std::move(p));
  }

  void delayed_publish(uActor::PubSub::Publication&& p, uint32_t delay) {
    api->delayed_publish(std::move(p), delay);
  }

  void deffered_block_for(uActor::PubSub::Filter&& filter, uint32_t timeout) {
    waiting = true;
    pattern.filter = std::move(filter);
    _timeout = timeout;
  }

 private:
  uint32_t _id;

  bool waiting = false;
  Pattern pattern;
  uint32_t _timeout = UINT32_MAX;

  std::string _node_id;
  std::string _actor_type;
  std::string _instance_id;
  std::string _code;

  std::deque<uActor::PubSub::Publication> message_queue;
  std::set<uint32_t> subscriptions;
  ExecutorApi* api;

  bool _initialized = false;

  void publish_exit_message(std::string exit_reason);
  void publish_creation_message();
  void add_default_subscription();
};
#endif  //  MAIN_INCLUDE_MANAGED_ACTOR_HPP_
