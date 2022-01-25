#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "managed_actor.hpp"
#include "native_actor_registry.hpp"
#include "pubsub/publication.hpp"

namespace uActor::ActorRuntime {

class ManagedNativeActor;

class NativeActor {
 public:
  NativeActor(ManagedNativeActor* actor_wrapper, std::string_view node_id,
              std::string_view actor_type, std::string_view instance_id);
  virtual void receive(const PubSub::Publication& publication) = 0;
  virtual ~NativeActor() {}

  template <typename T>
  static std::unique_ptr<NativeActor> create_instance(
      ActorRuntime::ManagedNativeActor* actor_wrapper, std::string_view node_id,
      std::string_view actor_type, std::string_view instance_id) {
    return std::make_unique<T>(actor_wrapper, node_id, actor_type, instance_id);
  }

 protected:
  void publish(PubSub::Publication&& publication);
  void republish(PubSub::Publication&& publication);
  void delayed_publish(PubSub::Publication&& publication, uint32_t delay);
  void reply(PubSub::Publication&& publication);
  uint32_t subscribe(PubSub::Filter&& filter,
                     PubSub::SubscriptionArguments arguments =
                         PubSub::SubscriptionArguments());
  void unsubscribe(uint32_t subscription_id);
  uint32_t add_reply_subscription();
  void deffered_block_for(PubSub::Filter&& filter, uint32_t timeout);
  uint32_t now();
  void enqueue_wakeup(uint32_t delay, std::string_view wakeup_id);

  std::string_view node_id() { return std::string_view(_node_id); }
  std::string_view actor_type() { return std::string_view(_actor_type); }
  std::string_view instance_id() { return std::string_view(_instance_id); }

 private:
  ManagedNativeActor* actor_wrapper;
  std::string _node_id;
  std::string _actor_type;
  std::string _instance_id;
};

}  //  namespace uActor::ActorRuntime
