#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_CODE_STORE_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_CODE_STORE_HPP_

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "code_handle.hpp"
#include "code_identifier.hpp"

namespace uActor::ActorRuntime {

class CodeStore {
 public:
  static CodeStore& get_instance() {
    static CodeStore instance;
    return instance;
  }

  std::optional<CodeHandle> retrieve(const CodeIdentifier& identifier);

  void store(CodeIdentifier&& identifier, std::string_view code,
             uint32_t lifetime_end);

  void cleanup();

 private:
  std::unordered_map<CodeIdentifier, std::pair<std::string, uint32_t>,
                     CodeIdentifierHasher>
      _store;
  std::mutex mtx;
};
}  // namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_CODE_STORE_HPP_
