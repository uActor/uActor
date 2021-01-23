#pragma once

#include <algorithm>
#include <cstdint>
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
  void receive_store(const PubSub::Publication &publication);

  void receive_retrieve(const PubSub::Publication &publication);

  void cleanup();

 private:
};

}  // namespace uActor::ActorRuntime
