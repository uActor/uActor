#include "native_actor.hpp"

#include "board_functions.hpp"
#include "managed_native_actor.hpp"

NativeActor::NativeActor(ManagedNativeActor* actor_wrapper,
                         std::string_view node_id, std::string_view actor_type,
                         std::string_view instance_id)
    : actor_wrapper(actor_wrapper),
      _node_id(node_id),
      _actor_type(actor_type),
      _instance_id(instance_id) {}

void NativeActor::publish(uActor::PubSub::Publication&& p) {
  actor_wrapper->publish(std::move(p));
}

void NativeActor::delayed_publish(uActor::PubSub::Publication&& publication,
                                  uint32_t delay) {
  actor_wrapper->delayed_publish(std::move(publication), delay);
}

uint32_t NativeActor::subscribe(uActor::PubSub::Filter&& filter) {
  return actor_wrapper->subscribe(std::move(filter));
}

void NativeActor::unsubscribe(uint32_t subscription_id) {
  actor_wrapper->unsubscribe(subscription_id);
}

uint32_t NativeActor::now() { return BoardFunctions::timestamp(); }

void NativeActor::deffered_block_for(uActor::PubSub::Filter&& filter,
                                     uint32_t timeout) {
  actor_wrapper->deffered_block_for(std::move(filter), timeout);
}
