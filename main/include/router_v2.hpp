#ifndef MAIN_INCLUDE_ROUTER_V2_HPP_
#define MAIN_INCLUDE_ROUTER_V2_HPP_

#include <optional>
#include "message.hpp"

class RouterV2 {
 public:
  class Internal;
  static RouterV2& getInstance() {
    static RouterV2 instance;
    return instance;
  }

  RouterV2();
  ~RouterV2();

  void send(uint64_t sender, uint64_t receiver, Message&& message);
  void register_actor(const uint64_t actor_id);
  void deregister_actor(uint64_t actor_id);
  void register_alias(const uint64_t actor_id, const uint64_t alias_id);
  std::optional<Message> receive(uint64_t receiver, size_t wait_time = 0);

 private:
  Internal* _internal;
};

#endif  // MAIN_INCLUDE_ROUTER_V2_HPP_
