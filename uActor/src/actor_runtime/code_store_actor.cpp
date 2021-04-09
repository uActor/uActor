#include "actor_runtime/code_store_actor.hpp"

#include "support/logger.hpp"

namespace uActor::ActorRuntime {
CodeStoreActor::CodeStoreActor(ManagedNativeActor* actor_wrapper,
                               std::string_view node_id,
                               std::string_view actor_type,
                               std::string_view instance_id)
    : ActorRuntime::NativeActor(actor_wrapper, node_id, actor_type,
                                instance_id) {}

void CodeStoreActor::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "init") {
    uActor::Support::Logger::trace("CODE-STORE", "INIT", "Init code store");
    subscribe(PubSub::Filter{
        PubSub::Constraint{"publisher_node_id", std::string(node_id())},
        PubSub::Constraint{"type", "actor_code"}});
    subscribe(PubSub::Filter{
        PubSub::Constraint{"publisher_node_id", std::string(node_id())},
        PubSub::Constraint{"type", "actor_code_lifetime_update"}});
    code_fetch_subscription_id = subscribe(
        PubSub::Filter{
            PubSub::Constraint{"type", "code_fetch_request"},
            PubSub::Constraint{"publisher_node_id", node_id()},
            PubSub::Constraint{"last_cache_miss_node_id", node_id(),
                               PubSub::ConstraintPredicates::NE, true}},
        1);
    subscribe(
        PubSub::Filter{PubSub::Constraint{"type", "code_unavailable"},
                       PubSub::Constraint{"publisher_node_id", node_id()}});
    subscribe(PubSub::Filter{
        PubSub::Constraint{"type", "remote_fetch_control_command"},
        PubSub::Constraint{"publisher_node_id", node_id()}});
    enqueue_wakeup(30000, "periodic_cleanup");
  } else if (publication.get_str_attr("type") == "wakeup" &&
             publication.get_str_attr("wakeup_id") == "periodic_cleanup") {
    cleanup();
  } else if (publication.get_str_attr("type") == "actor_code") {
    receive_code_store(publication);
  } else if (publication.get_str_attr("type") == "actor_code_lifetime_update") {
    receive_lifetime_update(publication);
  } else if (publication.get_str_attr("type") == "code_unavailable") {
    receive_code_unavailable_notification(publication);
  } else if (publication.get_str_attr("type") == "code_fetch_request") {
    receive_code_fetch_request(PubSub::Publication(publication));
  } else if (publication.get_str_attr("type") == "code_fetch_response") {
    receive_code_fetch_response(publication);
  } else if (publication.get_str_attr("type") ==
             "remote_fetch_control_command") {
    receive_remote_fetch_control_command(publication);
  }
}

void CodeStoreActor::receive_code_store(
    const PubSub::Publication& publication) {
  if (publication.has_attr("actor_code_type") &&
      publication.has_attr("actor_code_version") &&
      publication.has_attr("actor_code_runtime_type") &&
      publication.has_attr("actor_code_lifetime_end") &&
      publication.has_attr("actor_code")) {
    uActor::Support::Logger::trace("CODE-STORE", "STORE",
                                   "Received code package");
    CodeStore::get_instance().insert_or_refresh(
        CodeIdentifier(*publication.get_str_attr("actor_code_type"),
                       *publication.get_str_attr("actor_code_version"),
                       *publication.get_str_attr("actor_code_runtime_type")),
        *publication.get_int_attr("actor_code_lifetime_end"),
        *publication.get_str_attr("actor_code"));
  } else {
    uActor::Support::Logger::trace("CODE-STORE", "STORE",
                                   "Received incomplete code package");
  }
}

void CodeStoreActor::receive_lifetime_update(
    const PubSub::Publication& publication) {
  if (!(publication.has_attr("actor_code_type") &&
        publication.has_attr("actor_code_version") &&
        publication.has_attr("actor_code_runtime_type") &&
        publication.has_attr("actor_code_lifetime_end"))) {
    return;
  }

  CodeStore::get_instance().insert_or_refresh(
      CodeIdentifier(*publication.get_str_attr("actor_code_type"),
                     *publication.get_str_attr("actor_code_version"),
                     *publication.get_str_attr("actor_code_runtime_type")),
      *publication.get_int_attr("actor_code_lifetime_end"), std::nullopt);
}

void CodeStoreActor::receive_code_unavailable_notification(
    const PubSub::Publication& publication) {
  if (!(publication.has_attr("unavailable_actor_type") &&
        publication.has_attr("unavailable_actor_version") &&
        publication.has_attr("unavailable_actor_version"))) {
    return;
  }
  Support::Logger::info(
      "CODE_STORE_ACTOR", "MISSING_CODE", "Code for actor %s missing",
      publication.get_str_attr("unavailable_actor_type")->data());
  std::string_view actor_type =
      *publication.get_str_attr("unavailable_actor_type");
  std::string_view actor_version =
      *publication.get_str_attr("unavailable_actor_version");
  std::string_view actor_runtime_type =
      *publication.get_str_attr("unavailable_actor_runtime_type");

  auto [debounce_it, inserted] = code_miss_debounce.try_emplace(
      CodeIdentifier(actor_type, actor_version, actor_runtime_type),
      (now() + 5000));

  if (inserted || debounce_it->second < now()) {
    debounce_it->second = now() + 5000;
    auto code_fetch =
        PubSub::Publication(node_id(), this->actor_type(), instance_id());
    code_fetch.set_attr("type", "code_fetch_request");
    code_fetch.set_attr("fetch_actor_type", actor_type);
    code_fetch.set_attr("fetch_actor_version", actor_version);
    code_fetch.set_attr("fetch_actor_runtime_type", actor_runtime_type);
    code_fetch.set_attr("last_cache_miss_node_id", node_id());
    publish(std::move(code_fetch));
  } else {
    printf("Debounce\n");
  }
}

void CodeStoreActor::receive_code_fetch_request(
    PubSub::Publication&& publication) {
  if (!(publication.has_attr("fetch_actor_type") &&
        publication.has_attr("fetch_actor_version") &&
        publication.has_attr("fetch_actor_runtime_type"))) {
    return;
  }

  std::string_view actor_type = *publication.get_str_attr("fetch_actor_type");
  std::string_view actor_version =
      *publication.get_str_attr("fetch_actor_version");
  std::string_view actor_runtime_type =
      *publication.get_str_attr("fetch_actor_runtime_type");

  auto response =
      PubSub::Publication(node_id(), this->actor_type(), instance_id());

  {
    auto local_code = CodeStore::get_instance().retrieve(
        CodeIdentifier(actor_type, actor_version, actor_runtime_type), false);

    if (local_code) {
      response.set_attr("fetch_actor_code", local_code->code);
    } else {
      auto [debounce_it, inserted] = code_miss_debounce.try_emplace(
          CodeIdentifier(actor_type, actor_version, actor_runtime_type),
          (now() + 5000));
      if (inserted || debounce_it->second < now()) {
        debounce_it->second = now() + 5000;
        Support::Logger::info("CODE_STORE_ACTOR", "CODE_FETCH",
                              "Code Missing, resubmitting");
        publication.set_attr("last_cache_miss_node_id", node_id());
        republish(std::move(publication));
      }
      return;
    }
  }

  response.set_attr("type", "code_fetch_response");
  response.set_attr("node_id", *publication.get_str_attr("publisher_node_id"));
  response.set_attr("actor_type",
                    *publication.get_str_attr("publisher_actor_type"));
  response.set_attr("instance_id",
                    *publication.get_str_attr("publisher_instance_id"));
  response.set_attr("fetch_actor_type", actor_type);
  response.set_attr("fetch_actor_version", actor_version);
  response.set_attr("fetch_actor_runtime_type", actor_runtime_type);
  publish(std::move(response));
}

void CodeStoreActor::receive_code_fetch_response(
    const PubSub::Publication& publication) {
  if (!(publication.has_attr("fetch_actor_type") &&
        publication.has_attr("fetch_actor_version") &&
        publication.has_attr("fetch_actor_code") &&
        publication.has_attr("fetch_actor_runtime_type"))) {
    return;
  }

  std::string_view actor_type = *publication.get_str_attr("fetch_actor_type");
  std::string_view actor_version =
      *publication.get_str_attr("fetch_actor_version");
  std::string_view actor_code = *publication.get_str_attr("fetch_actor_code");
  std::string_view actor_runtime_type =
      *publication.get_str_attr("fetch_actor_runtime_type");

  CodeStore::get_instance().insert_or_refresh(
      CodeIdentifier(actor_type, actor_version, actor_runtime_type),
      CodeStore::MAGIC_LIFETIME_VALUE_CACHE_OR_EXISTING, actor_code);
}

void CodeStoreActor::receive_remote_fetch_control_command(
    const PubSub::Publication& publication) {
  if (!(publication.has_attr("command"))) {
    return;
  }

  auto command = *publication.get_str_attr("command");

  if (command == "enable") {
    unsubscribe(code_fetch_subscription_id);
    code_fetch_subscription_id = subscribe(
        PubSub::Filter{
            PubSub::Constraint{"type", "code_fetch_request"},
            PubSub::Constraint{"last_cache_miss_node_id", node_id(),
                               PubSub::ConstraintPredicates::NE, true}},
        1);
  }

  if (command == "disable") {
    unsubscribe(code_fetch_subscription_id);
    code_fetch_subscription_id = subscribe(
        PubSub::Filter{
            PubSub::Constraint{"type", "code_fetch_request"},
            PubSub::Constraint{"publisher_node_id", node_id()},
            PubSub::Constraint{"last_cache_miss_node_id", node_id(),
                               PubSub::ConstraintPredicates::NE, true}},
        1);
  }
}

void CodeStoreActor::cleanup() {
  std::vector<CodeIdentifier> to_delete;
  for (auto& pair : code_miss_debounce) {
    if (pair.second < now()) {
      to_delete.emplace_back(pair.first);
    }
  }
  for (const auto& key : to_delete) {
    code_miss_debounce.erase(key);
  }
  to_delete.clear();

  CodeStore::get_instance().cleanup();
  enqueue_wakeup(30000, "periodic_cleanup");
}
}  // namespace uActor::ActorRuntime
