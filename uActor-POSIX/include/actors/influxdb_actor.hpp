#pragma once

#include <InfluxDB.h>
#include <InfluxDBFactory.h>

#include <memory>

#include "actor_runtime/native_actor.hpp"
#include "support/string_helper.hpp"

namespace uActor::Database {
class InfluxDBActor : public ActorRuntime::NativeActor {
 public:
  InfluxDBActor(ActorRuntime::ManagedNativeActor* actor_wrapper,
                std::string_view node_id, std::string_view actor_type,
                std::string_view instance_id)
      : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {}

  ~InfluxDBActor() = default;

  void receive(const PubSub::Publication& publication) override;

 private:
  void receive_data_point(const PubSub::Publication& publication);

  std::unique_ptr<influxdb::InfluxDB> connection =
      influxdb::InfluxDBFactory::Get(
          "http://"
          "smart_office:smart_office_user@influxdb:8086?db=smart_office");
};
}  // namespace uActor::Database
