#ifndef MAIN_INCLUDE_NATIVE_ACTOR_HPP_
#define MAIN_INCLUDE_NATIVE_ACTOR_HPP_

#include <string>
#include <string_view>

#include "managed_actor.hpp"
#include "pubsub/publication.hpp"

class ManagedNativeActor;

class NativeActor {
 public:
  NativeActor(ManagedNativeActor* actor_wrapper, std::string_view node_id,
              std::string_view actor_type, std::string_view instance_id);
  virtual void receive(const uActor::PubSub::Publication& publication) = 0;
  virtual ~NativeActor() {}

 protected:
  void publish(uActor::PubSub::Publication&& publication);
  void delayed_publish(uActor::PubSub::Publication&& publication,
                       uint32_t delay);
  uint32_t subscribe(uActor::PubSub::Filter&& filter);
  void unsubscribe(uint32_t subscription_id);
  void deffered_block_for(uActor::PubSub::Filter&& filter, uint32_t timeout);
  uint32_t now();

  std::string_view node_id() { return std::string_view(_node_id); }
  std::string_view actor_type() { return std::string_view(_actor_type); }
  std::string_view instance_id() { return std::string_view(_instance_id); }

 private:
  ManagedNativeActor* actor_wrapper;
  std::string _node_id;
  std::string _actor_type;
  std::string _instance_id;
};

#endif  // MAIN_INCLUDE_NATIVE_ACTOR_HPP_
