#ifndef UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_ACTOR_HPP_
#define UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_ACTOR_HPP_

#include <string>
#include <string_view>

#include "actor_runtime/native_actor.hpp"
#include "telemetry_data.hpp"

namespace uActor::Controllers {

class TelemetryActor : public ActorRuntime::NativeActor {
 public:
  // TODO(raphaelhetzel) not a fan of using a static here,
  // evaluate ways of passing data to native actors.
  static std::function<void()> telemetry_fetch_hook;

  TelemetryActor(ActorRuntime::ManagedNativeActor* actor_wrapper,
                 std::string_view node_id, std::string_view actor_type,
                 std::string_view instance_id)
      : ActorRuntime::NativeActor(actor_wrapper, node_id, actor_type,
                                  instance_id) {}

  void receive(const PubSub::Publication& publication);

 private:
  void try_publish_telemetry_datapoint(const TelemetryData& data);
};

}  // namespace uActor::Controllers

#endif  // UACTOR_INCLUDE_CONTROLLERS_TELEMETRY_ACTOR_HPP_
