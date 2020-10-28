#ifndef UACTOR_ESP32_COMPONENTS_SCD30_ACTOR_SCD30_ACTOR_HPP_
#define UACTOR_ESP32_COMPONENTS_SCD30_ACTOR_SCD30_ACTOR_HPP_

#include <scd30.h>

#include <string>

#include "actor_runtime/native_actor.hpp"
#include "support/logger.hpp"

namespace uActor::ESP32::Sensors {

class SCD30Actor : public ActorRuntime::NativeActor {
 public:
  SCD30Actor(ActorRuntime::ManagedNativeActor* actor_wrapper,
              std::string_view node_id, std::string_view actor_type,
              std::string_view instance_id);

  ~SCD30Actor() override;

  void receive(const PubSub::Publication& publication) override;

 private:
  uint32_t ready_retries = 0;
  std::set<std::string> registered_actors;
  
  void fetch_send_updates();
  
  void send_update(std::string variable, std::string unit, float value);
  void send_failure_notification();
  void send_delayed_trigger(uint32_t delay, std::string type);
  void send_exit_message();

  void register_unmanaged_actor(std::string type);
  void deregister_managed_actor(std::string type);
};

}  // namespace uActor::ESP32::Sensors

#endif  //  UACTOR_ESP32_COMPONENTS_SCD30_ACTOR_SCD30_ACTOR_HPP_
