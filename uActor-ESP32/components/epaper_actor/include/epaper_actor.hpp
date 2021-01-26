#ifndef UACTOR_ESP32_COMPONENTS_EPAPER_ACTOR_INCLUDE_EPAPER_ACTOR_HPP_
#define UACTOR_ESP32_COMPONENTS_EPAPER_ACTOR_INCLUDE_EPAPER_ACTOR_HPP_

#include <gdeh0213b73.h>
extern "C" {
#include <Fonts/FreeMono9pt7b.h>
}

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "actor_runtime/native_actor.hpp"
#include "board_functions.hpp"
#include "pubsub/publication.hpp"

namespace uActor::ESP32::Notifications {
class EPaperNotificationActor : public ActorRuntime::NativeActor {
 public:
  struct State {
    State(std::string notification_id, std::string node_id,
          std::string notification_text, size_t number_of_notifications)
        : notification_id(std::move(notification_id)),
          node_id(std::move(node_id)),
          notification_text(std::move(notification_text)),
          number_of_notifications(number_of_notifications) {}

    State() = default;

    std::string notification_id;
    std::string node_id;
    std::string notification_text;
    size_t number_of_notifications = -1;

    bool operator==(const State& other) const {
      return notification_id == notification_id && node_id == other.node_id &&
             number_of_notifications == other.number_of_notifications &&
             notification_text == other.notification_text;
    }
  };

  EPaperNotificationActor(ActorRuntime::ManagedNativeActor* actor_wrapper,
                          std::string_view node_id, std::string_view actor_type,
                          std::string_view instance_id);

  void receive(const PubSub::Publication& publication) override;

  void receive_notification(const PubSub::Publication& publication);

  void receive_notification_cancelation(const PubSub::Publication& publication);

  void init();

  void exit();

  void print_next(std::optional<State> state_input = std::nullopt);

  void cleanup(std::optional<State> state_input = std::nullopt);

  void update(State&& state, bool force = false);

 private:
  struct Notification {
    Notification(std::string id, std::string text, uint32_t lifetime)
        : id(id), text(std::move(text)), lifetime(lifetime) {}
    std::string id;
    std::string text;
    uint32_t lifetime;
  };

  std::map<std::string, Notification> notifications;
  State current_state;
};

}  // namespace uActor::ESP32::Notifications

#endif  // UACTOR_ESP32_COMPONENTS_EPAPER_ACTOR_INCLUDE_EPAPER_ACTOR_HPP_
