#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "code_handle.hpp"
#include "code_identifier.hpp"

namespace uActor::ActorRuntime {

struct CodePacket {
  explicit CodePacket(uint32_t lifetime_end)
      : lifetime_end(lifetime_end), code_available(false) {}

  CodePacket(uint32_t lifetime_end, std::string&& code)
      : lifetime_end(lifetime_end),
        code_available(true),
        code(std::move(code)) {}

  uint32_t lifetime_end;
  bool code_available;
  std::string code;
};

class CodeStore {
 public:
  constexpr static uint32_t MAGIC_LIFETIME_VALUE_INFINTE = 0;
  constexpr static uint32_t MAGIC_LIFETIME_VALUE_CACHE_OR_EXISTING = 1;

  // todo(raphaelhetzel) Implement Cache
  constexpr static uint32_t CACHE_CAPACITY = 0;

  static CodeStore& get_instance() {
    static CodeStore instance;
    return instance;
  }

  std::optional<CodeHandle> retrieve(const CodeIdentifier& identifier,
                                     bool use_hook = true);

  void insert_or_refresh(CodeIdentifier&& identifier, uint32_t lifetime_end,
                         std::optional<std::string_view> code);

  void cleanup();

  void code_unavailable_hook(const CodeIdentifier& code_identifier);

 private:
  std::unordered_map<CodeIdentifier, CodePacket, CodeIdentifierHasher> _store;
  std::mutex mtx;
};
}  // namespace uActor::ActorRuntime
