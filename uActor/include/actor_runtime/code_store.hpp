#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_CODE_STORE_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_CODE_STORE_HPP_

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "board_functions.hpp"
#include "code_identifier.hpp"
#include "native_actor.hpp"

namespace uActor::ActorRuntime {
class CodeStore : public NativeActor {
 public:
  CodeStore(ManagedNativeActor *actor_wrapper, std::string_view node_id,
            std::string_view actor_type, std::string_view instance_id);

  void receive(const PubSub::Publication &publication) override;
  void receive_store(const PubSub::Publication &publication);

  void store(std::string_view type, std::string_view version,
             std::string_view runtime_type, std::string_view code,
             uint32_t lifetime_end);

  void receive_retrieve(const PubSub::Publication &publication);

  std::optional<std::string> retrieve(std::string_view type,
                                      std::string_view version,
                                      std::string_view runtime_type);

  void cleanup();

 private:
  std::unordered_map<CodeIdentifier, std::pair<std::string, uint32_t>,
                     CodeIdentifierHasher>
      _store;
};

}  // namespace uActor::ActorRuntime
#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_CODE_STORE_HPP_
