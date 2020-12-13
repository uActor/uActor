#include "actor_runtime/code_store_actor.hpp"

#include "support/logger.hpp"

namespace uActor::ActorRuntime {
CodeStoreActor::CodeStoreActor(ManagedNativeActor* actor_wrapper,
                               std::string_view node_id,
                               std::string_view actor_type,
                               std::string_view instance_id)
    : ActorRuntime::NativeActor(actor_wrapper, node_id, actor_type,
                                instance_id) {
  subscribe(PubSub::Filter{
      PubSub::Constraint{"publisher_node_id", std::string(node_id)},
      PubSub::Constraint{"type", "actor_code"}});
  subscribe(
      PubSub::Filter{PubSub::Constraint{"command", "fetch_actor_code"},
                     PubSub::Constraint{"node_id", std::string(node_id)}});
  uActor::Support::Logger::trace("CODE-STORE", "INIT", "Init code store");
}

void CodeStoreActor::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "init" ||
      publication.get_str_attr("command") == "periodic_cleanup") {
    cleanup();
  } else if (publication.get_str_attr("type") == "actor_code") {
    receive_store(publication);
  } else if (publication.get_str_attr("command") == "fetch_actor_code") {
    receive_retrieve(publication);
  }
}

void CodeStoreActor::receive_store(const PubSub::Publication& publication) {
  if (publication.has_attr("actor_code_type") &&
      publication.has_attr("actor_code_version") &&
      publication.has_attr("actor_code_runtime_type") &&
      publication.has_attr("actor_code_lifetime_end") &&
      publication.has_attr("actor_code")) {
    uActor::Support::Logger::trace("CODE-STORE", "STORE",
                                   "Received code package");
    CodeStore::get_instance().store(
        CodeIdentifier(*publication.get_str_attr("actor_code_type"),
                       *publication.get_str_attr("actor_code_version"),
                       *publication.get_str_attr("actor_code_runtime_type")),
        *publication.get_str_attr("actor_code"),
        *publication.get_int_attr("actor_code_lifetime_end"));
  } else {
    uActor::Support::Logger::trace("CODE-STORE", "STORE",
                                   "Received incomplete code package");
  }
}

void CodeStoreActor::receive_retrieve(const PubSub::Publication& publication) {
  uActor::Support::Logger::trace("CODE-STORE", "RETRIEVE", "Retrieve request");
  if (publication.has_attr("actor_code_type") &&
      publication.has_attr("actor_code_version") &&
      publication.has_attr("actor_code_runtime_type")) {
    auto response = PubSub::Publication(node_id(), actor_type(), instance_id());

    {  // Minimize Lock Time (calling pusblish while holding a handle results in
       // a deadlock)
      auto code_handle = CodeStore::get_instance().retrieve(
          CodeIdentifier(*publication.get_str_attr("actor_code_type"),
                         *publication.get_str_attr("actor_code_version"),
                         *publication.get_str_attr("actor_code_runtime_type")));
      if (code_handle) {
        response.set_attr("actor_code", code_handle->code);
      } else {
        return;
      }
    }
    uActor::Support::Logger::trace("CODE-STORE", "RETRIEVE",
                                   "Found code package");
    response.set_attr("node_id",
                      *publication.get_str_attr("publisher_node_id"));
    response.set_attr("actor_type",
                      *publication.get_str_attr("publisher_actor_type"));
    response.set_attr("instance_id",
                      *publication.get_str_attr("publisher_instance_id"));
    response.set_attr("type", "fetch_actor_code_response");
    response.set_attr("actor_code_type",
                      *publication.get_str_attr("actor_code_type"));
    response.set_attr("actor_code_version",
                      *publication.get_str_attr("actor_code_version"));
    publish(std::move(response));
  } else {
    uActor::Support::Logger::trace("CODE-STORE", "RETRIEVE",
                                   "Code package not found");
  }
}

void CodeStoreActor::cleanup() {
  CodeStore::get_instance().cleanup();

  auto retrigger_msg =
      PubSub::Publication(node_id(), actor_type(), instance_id());
  retrigger_msg.set_attr("node_id", node_id());
  retrigger_msg.set_attr("actor_type", actor_type());
  retrigger_msg.set_attr("instance_id", instance_id());
  retrigger_msg.set_attr("command", "periodic_cleanup");
  delayed_publish(std::move(retrigger_msg), 30000);
}
}  // namespace uActor::ActorRuntime
