#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "board_functions.hpp"
#include "code_identifier.hpp"
#include "code_store.hpp"
#include "native_actor.hpp"

namespace uActor::ActorRuntime {

class CodeStoreActor : public NativeActor {
 public:
  CodeStoreActor(ManagedNativeActor *actor_wrapper, std::string_view node_id,
                 std::string_view actor_type, std::string_view instance_id);

  void receive(const PubSub::Publication &publication) override;
  void receive_code_store(const PubSub::Publication &publication);
  void receive_code_fetch(const PubSub::Publication &publication);
  void receive_lifetime_update(const PubSub::Publication &publication);

  void receive_code_unavailable_notification(
      const PubSub::Publication &publication);
  void receive_remote_code_fetch_request(
      const PubSub::Publication &publication);
  void receive_remote_code_fetch_response(
      const PubSub::Publication &publication);

  void receive_remote_fetch_control_command(
      const PubSub::Publication &publication);

  void cleanup();

 private:
  std::map<CodeIdentifier, uint32_t> code_miss_debounce;
  uint32_t remote_code_fetch_subscription_id = 0;
};

}  // namespace uActor::ActorRuntime
