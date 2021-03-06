#pragma once

#include <InfluxDB.h>
#include <InfluxDBFactory.h>

#include <memory>
#include <string>

#include "actor_runtime/native_actor.hpp"
#include "support/string_helper.hpp"

namespace uActor::Database {
class InfluxDBActor : public ActorRuntime::NativeActor {
 public:
  // TODO(raphaelhetzel) This should be per-instane.
  // Change once we introduce init parameters.
  static std::string server_url;

  InfluxDBActor(ActorRuntime::ManagedNativeActor* actor_wrapper,
                std::string_view node_id, std::string_view actor_type,
                std::string_view instance_id)
      : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {}

  ~InfluxDBActor() = default;

  void receive(const PubSub::Publication& publication) override;

 private:
  void receive_data_point(const PubSub::Publication& publication);
};
}  // namespace uActor::Database
