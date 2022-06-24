#pragma once

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif

#include <cstdint>
#include <deque>
#include <functional>
#include <set>
#include <string>
#include <utility>

#include "executor_api.hpp"
#include "pubsub/filter.hpp"
#include "pubsub/publication.hpp"
#include "pubsub/router.hpp"
#include "reply_context.hpp"
#include "runtime_allocator_configuration.hpp"
#include "support/tracking_allocator.hpp"

namespace uActor::ActorRuntime {

class ManagedActor {
 public:
  enum class RuntimeReturnValue {
    NONE = 0,
    OK,
    NOT_READY,
    RUNTIME_ERROR,
    INITIALIZATION_ERROR
  };

  template <typename U>
  using Allocator = RuntimeAllocatorConfiguration::Allocator<U>;

  using allocator_type = Allocator<ManagedActor>;

  template <typename U>
  constexpr static auto make_allocator =
      RuntimeAllocatorConfiguration::make_allocator<U>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  struct ReceiveResult {
    ReceiveResult(bool exit, uint32_t next_timeout)
        : exit(exit), next_timeout(next_timeout) {}
    bool exit;
    uint32_t next_timeout;
  };

  ManagedActor(const ManagedActor&) = delete;

  template <typename PAllocator = allocator_type>
  ManagedActor(ExecutorApi* api, uint32_t unique_id, std::string_view node_id,
               std::string_view actor_type, std::string_view actor_version,
               std::string_view instance_id,
               PAllocator allocator = make_allocator<ManagedActor>())
      : reply_context(allocator),
        _id(unique_id),
        _node_id(node_id, allocator),
        _actor_type(actor_type, allocator),
        _actor_version(actor_version, allocator),
        _instance_id(instance_id, allocator),
        message_queue(allocator),
        subscriptions(allocator),
        api(api) {}

  ~ManagedActor() {
    for (uint32_t sub_id : subscriptions) {
      api->remove_subscription(_id, sub_id);
    }
#if CONFIG_UACTOR_ENABLE_TELEMETRY
    total_queue_size -= queue_size();
#endif
  }

  virtual RuntimeReturnValue receive(PubSub::Publication&& p) = 0;

  virtual std::string actor_runtime_type() = 0;

  std::pair<RuntimeReturnValue, uint32_t> early_initialize();

  RuntimeReturnValue late_initialize(std::string&& code);

  bool hibernate() {
    if (hibernate_internal()) {
      _initialized = false;
      return true;
    }
    return false;
  }

  RuntimeReturnValue wakeup() {
    auto ret = wakeup_internal();
    if (ret == RuntimeReturnValue::OK) {
      _initialized = true;
    }
    return ret;
  }

  ReceiveResult receive_next_internal();

  bool enqueue(PubSub::Publication&& message);

  void trigger_timeout(bool user_defined, std::string_view wakeup_id);

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

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  static std::atomic<int> total_queue_size;
#endif

  uint32_t add_reply_subscription();
  void publish_creation_message();

 protected:
  virtual RuntimeReturnValue early_internal_initialize() = 0;
  virtual RuntimeReturnValue late_internal_initialize(std::string&& code) = 0;
  virtual bool hibernate_internal() = 0;
  virtual RuntimeReturnValue wakeup_internal() = 0;

  uint32_t subscribe(PubSub::Filter&& f,
                     PubSub::SubscriptionArguments arguments);
  void unsubscribe(uint32_t sub_id);

  void publish(PubSub::Publication&& p) const;
  void republish(PubSub::Publication&& p);
  void delayed_publish(PubSub::Publication&& p, uint32_t delay);
  void enqueue_wakeup(uint32_t delay, std::string_view wakeup_id);

  void reply(PubSub::Publication&& p) {
    PubSub::Publication wrapper{_node_id, _actor_type, _instance_id};
    wrapper.set_attr("type", "__unicast_wrapper");
    wrapper.set_attr("__wrapped", p.root_handle());
    reply_context.add_reply_fields(&wrapper);
    publish(std::move(p));
  }

  void request(PubSub::Filter&& f) {
    PubSub::SubscriptionArguments arguments;
    arguments.fetch_policy = PubSub::FetchPolicy::FETCH;
    subscribe(std::move(f), std::move(arguments));
  }

  void deffered_block_for(PubSub::Filter&& filter, uint32_t timeout);

  uint32_t queue_size();
  ReplyContext<Allocator> reply_context;

 private:
  uint32_t _id;

  bool waiting = false;
  PubSub::Filter pattern;
  uint32_t _timeout = UINT32_MAX;

  AString _node_id;
  AString _actor_type;
  AString _actor_version;
  AString _instance_id;

  std::deque<PubSub::Publication, Allocator<PubSub::Publication>> message_queue;
  std::set<uint32_t, std::less<uint32_t>, Allocator<uint32_t>> subscriptions;
  ExecutorApi* api;

  bool _initialized = false;

  uint32_t code_fetch_retries = 0;
  bool waiting_for_code = false;

  void publish_exit_message(std::string exit_reason);
};

}  //  namespace uActor::ActorRuntime
