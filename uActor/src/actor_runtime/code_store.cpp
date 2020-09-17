#include "actor_runtime/code_store.hpp"

#include "support/logger.hpp"

namespace uActor::ActorRuntime {
CodeStore::CodeStore(ManagedNativeActor* actor_wrapper,
                     std::string_view node_id, std::string_view actor_type,
                     std::string_view instance_id)
    : ActorRuntime::NativeActor(actor_wrapper, node_id, actor_type,
                                instance_id) {
  subscribe(PubSub::Filter{
      PubSub::Constraint{"publisher_node_id", std::string(node_id)},
      PubSub::Constraint{"type", "actor_code"}});
  uActor::Support::Logger::trace("CODE-STORE", "INIT", "Init code store");
}

void CodeStore::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "init" ||
      publication.get_str_attr("command") == "periodic_cleanup") {
    cleanup();
  } else if (publication.get_str_attr("type") == "actor_code") {
    receive_store(publication);
  } else if (publication.get_str_attr("command") == "fetch_actor_code") {
    printf("Called\n");
    receive_retrieve(publication);
  }
}

void CodeStore::receive_store(const PubSub::Publication& publication) {
  if (publication.has_attr("actor_code_type") &&
      publication.has_attr("actor_code_version") &&
      publication.has_attr("actor_code_runtime_type") &&
      publication.has_attr("actor_code_lifetime_end") &&
      publication.has_attr("actor_code")) {
    uActor::Support::Logger::trace("CODE-STORE", "STORE",
                                   "Received code package");
    store(*publication.get_str_attr("actor_code_type"),
          *publication.get_str_attr("actor_code_version"),
          *publication.get_str_attr("actor_code_runtime_type"),
          *publication.get_str_attr("actor_code"),
          *publication.get_int_attr("actor_code_lifetime_end"));
  } else {
    uActor::Support::Logger::trace("CODE-STORE", "STORE",
                                   "Received incomplete code package");
  }
}

void CodeStore::store(std::string_view type, std::string_view version,
                      std::string_view runtime_type, std::string_view code,
                      uint32_t lifetime_end) {
  const auto& [iterator, inserted] =
      _store.try_emplace(CodeIdentifier(type, version, runtime_type),
                         std::make_pair(std::move(code), lifetime_end));
  uActor::Support::Logger::trace("CODE-STORE", "STORE",
                                 "Inserted code package");
  if (!inserted) {
    iterator->second.second = std::max(lifetime_end, iterator->second.second);
    uActor::Support::Logger::trace("CODE-STORE", "STORE",
                                   "Updated code package");
  }
}

void CodeStore::receive_retrieve(const PubSub::Publication& publication) {
  uActor::Support::Logger::trace("CODE-STORE", "RETRIEVE", "Retrieve request");
  if (publication.has_attr("actor_code_type") &&
      publication.has_attr("actor_code_version") &&
      publication.has_attr("actor_code_runtime_type")) {
    auto code = retrieve(*publication.get_str_attr("actor_code_type"),
                         *publication.get_str_attr("actor_code_version"),
                         *publication.get_str_attr("actor_code_runtime_type"));
    if (code) {
      uActor::Support::Logger::trace("CODE-STORE", "RETRIEVE",
                                     "Found code package");
      auto response =
          PubSub::Publication(node_id(), actor_type(), instance_id());
      response.set_attr("node_id",
                        *publication.get_str_attr("publisher_node_id"));
      response.set_attr("actor_type",
                        *publication.get_str_attr("publisher_actor_type"));
      response.set_attr("instance_id",
                        *publication.get_str_attr("publisher_instance_id"));
      response.set_attr("type", "fetch_actor_code_response");
      response.set_attr("actor_code_type",
                        *publication.get_str_attr("publisher_actor_type"));
      response.set_attr("actor_code_version",
                        *publication.get_str_attr("actor_code_version"));
      response.set_attr("actor_code", std::move(*code));
      publish(std::move(response));
    } else {
      uActor::Support::Logger::trace("CODE-STORE", "RETRIEVE",
                                     "Code package not found");
    }
  }
}

std::optional<std::string> CodeStore::retrieve(std::string_view type,
                                               std::string_view version,
                                               std::string_view runtime_type) {
  if (const auto& iterator =
          _store.find(CodeIdentifier(type, version, runtime_type));
      iterator != _store.end()) {
    return iterator->second.first;
  }
  return std::nullopt;
}

void CodeStore::cleanup() {
  uint32_t current_timestamp = BoardFunctions::timestamp();
  std::unordered_set<CodeIdentifier, CodeIdentifierHasher> to_delete;
  for (auto& [key, value_pair] : _store) {
    if (value_pair.second < current_timestamp) {
      to_delete.emplace(key);
    }
  }

  for (const auto& key : to_delete) {
    _store.erase(key);
    uActor::Support::Logger::trace("CODE-STORE", "DELETE",
                                   "Remove code package");
  }

  if (_store.load_factor() < 0.5 * _store.max_load_factor()) {
    _store.rehash(_store.size());
    uActor::Support::Logger::trace("CODE-STORE", "DELETE", "Rehash");
  }

  auto retrigger_msg =
      PubSub::Publication(node_id(), actor_type(), instance_id());
  retrigger_msg.set_attr("node_id", node_id());
  retrigger_msg.set_attr("actor_type", actor_type());
  retrigger_msg.set_attr("instance_id", instance_id());
  retrigger_msg.set_attr("command", "periodic_cleanup");
  delayed_publish(std::move(retrigger_msg), 30000);
}
}  // namespace uActor::ActorRuntime
