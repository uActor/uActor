#include "actor_runtime/code_store.hpp"

#include <algorithm>
#include <unordered_set>

#include "board_functions.hpp"
#include "pubsub/publication.hpp"
#include "pubsub/router.hpp"

namespace uActor::ActorRuntime {

std::optional<CodeHandle> CodeStore::retrieve(
    const CodeIdentifier& identifier) {
  std::unique_lock lck(mtx);
  auto stored_result = _store.find(identifier);
  if (stored_result != _store.end()) {
    if (stored_result->second.code_available) {
      return CodeHandle(std::string_view(stored_result->second.code),
                        std::move(lck));
    } else {
      code_unavailable_hook(identifier);
      return std::nullopt;
    }
  } else {
    return std::nullopt;
  }
}

void CodeStore::insert_or_refresh(CodeIdentifier&& identifier,
                                  uint32_t lifetime_end,
                                  std::optional<std::string_view> code) {
  std::unique_lock lck(mtx);
  auto [iterator, inserted] = _store.try_emplace(
      std::move(identifier), code ? CodePacket(lifetime_end, std::string(*code))
                                  : CodePacket(lifetime_end));
  if (!inserted) {
    auto& code_packet = iterator->second;
    if (lifetime_end == MAGIC_LIFETIME_VALUE_INFINTE) {
      iterator->second.lifetime_end;
    } else {
      iterator->second.lifetime_end =
          std::max(lifetime_end, iterator->second.lifetime_end);
    }
    if (code && !code_packet.code_available) {
      code_packet.code_available = true;
      code_packet.code = *code;
    }
  }
}

void CodeStore::cleanup() {
  std::unique_lock lck(mtx);
  uint32_t current_timestamp = BoardFunctions::timestamp();
  std::unordered_set<CodeIdentifier, CodeIdentifierHasher> to_delete;

  for (auto& [key, code_packet] : _store) {
    if (code_packet.lifetime_end > 0 &&
        code_packet.lifetime_end + 5000 < current_timestamp) {
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

// TODO(raphaelhetzel) Not a huge fan of sending a message from the CodeStore
// If this causes a deadlock in any way we might need to introduce a background
// thread
void CodeStore::code_unavailable_hook(const CodeIdentifier& identifier) {
  auto miss_message =
      PubSub::Publication(BoardFunctions::NODE_ID, "code_store", "1");
  miss_message.set_attr("type", "code_unavailable");
  miss_message.set_attr("unavailable_actor_type", identifier.type);
  miss_message.set_attr("unavailable_actor_version", identifier.version);
  miss_message.set_attr("unavailable_actor_runtime_type",
                        identifier.runtime_type);
  PubSub::Router::get_instance().publish(std::move(miss_message));
}

}  // namespace uActor::ActorRuntime
