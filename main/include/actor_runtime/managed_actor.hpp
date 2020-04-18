#ifndef MAIN_INCLUDE_ACTOR_RUNTIME_MANAGED_ACTOR_HPP_
#define MAIN_INCLUDE_ACTOR_RUNTIME_MANAGED_ACTOR_HPP_

#include <cstdint>
#include <deque>
#include <set>
#include <string>

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

  ManagedActor(ExecutorApi* api, uint32_t unique_id, const char* node_id,
               const char* actor_type, const char* instance_id,
               const char* code);

  ~ManagedActor() {
    for (uint32_t sub_id : subscriptions) {
      api->remove_subscription(_id, sub_id);
    }
  }

  virtual bool receive(const PubSub::Publication& p) = 0;

  bool initialize();

  ReceiveResult receive_next_internal();

  bool enqueue(PubSub::Publication&& message);

  void trigger_timeout();

  uint32_t id() { return _id; }

  const char* code() { return _code.c_str(); }

  const char* node_id() const { return _node_id.c_str(); }

  const char* actor_type() const { return _actor_type.c_str(); }

  const char* instance_id() const { return _instance_id.c_str(); }

  bool initialized() const { return _initialized; }

 protected:
  virtual bool internal_initialize() = 0;

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
  std::string _instance_id;
  std::string _code;

  std::deque<PubSub::Publication> message_queue;
  std::set<uint32_t> subscriptions;
  ExecutorApi* api;

  bool _initialized = false;

  void publish_exit_message(std::string exit_reason);
  void publish_creation_message();
  void add_default_subscription();
};

}  //  namespace uActor::ActorRuntime

#endif  //  MAIN_INCLUDE_ACTOR_RUNTIME_MANAGED_ACTOR_HPP_
