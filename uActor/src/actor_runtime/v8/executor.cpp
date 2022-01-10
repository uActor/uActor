#if CONFIG_UACTOR_V8
#include "actor_runtime/v8/executor.hpp"

#include "pubsub/publication.hpp"
#include "pubsub/router.hpp"

namespace uActor::ActorRuntime::V8Runtime {
void V8Executor::add_actor(uActor::PubSub::Publication&& publication) {
  add_actor_base<v8::Platform*>(publication, platform.get());
}

void V8Executor::register_actor_type() {
  PubSub::Publication p{BoardFunctions::NODE_ID, "v8_executor", "1"};
  p.set_attr("type", "executor_update");
  p.set_attr("command", "register");
  p.set_attr("actor_runtime_type", "javascript");
  p.set_attr("update_node_id", BoardFunctions::NODE_ID);
  p.set_attr("update_actor_type", "v8_executor");
  p.set_attr("update_instance_id", instance_id());
  p.set_attr("node_id", BoardFunctions::NODE_ID);
  PubSub::Router::get_instance().publish(std::move(p));
}

void V8Executor::deregister_actor_type() {
  PubSub::Publication p{BoardFunctions::NODE_ID, "v8_executor", "1"};
  p.set_attr("type", "executor_update");
  p.set_attr("command", "deregister");
  p.set_attr("actor_runtime_type", "javascript");
  p.set_attr("update_node_id", BoardFunctions::NODE_ID);
  p.set_attr("update_actor_type", "v8_executor");
  p.set_attr("update_instance_id", instance_id());
  p.set_attr("node_id", BoardFunctions::NODE_ID);
  uActor::PubSub::Router::get_instance().publish(std::move(p));
}

void V8Executor::start_v8() {
  // Start V8 as it is done in hello-world.cc
  // TODO(raphaelhetzel) Don't like that this is a global instance, search for
  // an alternative way to run V8.

  // hello-world.cc contains this initialization (related to i18n?
  // https://v8.dev/docs/i18n)
  // TODO(raphaelhetzel) Evaluate whether this is requried for our usecase
  // v8::V8::InitializeICUDefaultLocation(argv[0]);
  // v8::V8::InitializeExternalStartupData(argv[0]);
  if (!instance_running) {
    platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    instance_running = true;
  }
}

bool V8Executor::instance_running = false;
std::unique_ptr<v8::Platform> V8Executor::platform{nullptr};
}  // namespace uActor::ActorRuntime::V8Runtime
#endif
