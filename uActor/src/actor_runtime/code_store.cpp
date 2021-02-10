#include "actor_runtime/code_store.hpp"

#include <algorithm>
#include <unordered_set>

#include "board_functions.hpp"

namespace uActor::ActorRuntime {

std::optional<CodeHandle> CodeStore::retrieve(
    const CodeIdentifier& identifier) {
  std::unique_lock lck(mtx);
  auto stored_result = _store.find(identifier);
  if (stored_result != _store.end()) {
    return CodeHandle(std::string_view(stored_result->second.first),
                      std::move(lck));
  } else {
    return std::nullopt;
  }
}

void CodeStore::store(CodeIdentifier&& identifier, std::string_view code,
                      uint32_t lifetime_end) {
  std::unique_lock lck(mtx);
  auto [iterator, inserted] = _store.try_emplace(
      std::move(identifier), std::make_pair(code, lifetime_end));
  if (!inserted) {
    iterator->second.second = std::max(lifetime_end, iterator->second.second);
  }
}

void CodeStore::cleanup() {
  std::unique_lock lck(mtx);
  uint32_t current_timestamp = BoardFunctions::timestamp();
  std::unordered_set<CodeIdentifier, CodeIdentifierHasher> to_delete;
  for (auto& [key, value_pair] : _store) {
    if (value_pair.second > 0 && value_pair.second + 5000 < current_timestamp) {
      to_delete.emplace(key);
    }
  }

  for (const auto& key : to_delete) {
    _store.erase(key);
  }

  if (_store.load_factor() < 0.5 * _store.max_load_factor()) {
    _store.rehash(_store.size());
  }
}

}  // namespace uActor::ActorRuntime
