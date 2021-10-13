#pragma once

#include <utility>

#include "executor.hpp"
#include "managed_actor.hpp"
#include "managed_native_actor.hpp"

namespace uActor::ActorRuntime {

class NativeExecutor : public Executor<ManagedNativeActor, NativeExecutor> {
 public:
  NativeExecutor(uActor::PubSub::Router* router, const char* node_id,
                 const char* id)
      : Executor<ManagedNativeActor, NativeExecutor>(router, node_id,
                                                     "native_executor", id) {
    PubSub::Publication p{BoardFunctions::NODE_ID, "lua_executor", "1"};
    p.set_attr("type", "executor_update");
    p.set_attr("command", "register");
    p.set_attr("actor_runtime_type", "native");
    p.set_attr("update_node_id", BoardFunctions::NODE_ID);
    p.set_attr("update_actor_type", "native_executor");
    p.set_attr("update_instance_id", instance_id());
    p.set_attr("node_id", BoardFunctions::NODE_ID);
    PubSub::Router::get_instance().publish(std::move(p));
  }

  ~NativeExecutor() {
    PubSub::Publication p{BoardFunctions::NODE_ID, "native_executor", "1"};
    p.set_attr("type", "executor_update");
    p.set_attr("command", "deregister");
    p.set_attr("actor_runtime_type", "lua");
    p.set_attr("update_node_id", BoardFunctions::NODE_ID);
    p.set_attr("update_actor_type", "native_executor");
    p.set_attr("update_instance_id", instance_id());
    p.set_attr("node_id", BoardFunctions::NODE_ID);
    uActor::PubSub::Router::get_instance().publish(std::move(p));
    actors.clear();
  }

 private:
  void add_actor(PubSub::Publication&& publication) {
    add_actor_base(publication);
  }

  friend Executor;
};

}  //  namespace uActor::ActorRuntime
