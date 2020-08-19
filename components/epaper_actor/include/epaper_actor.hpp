#ifndef COMPONENTS_EPAPER_ACTOR_INCLUDE_EPAPER_NOTIFICATION_ACTOR_HPP_
#define COMPONENTS_EPAPER_ACTOR_INCLUDE_EPAPER_NOTIFICATION_ACTOR_HPP_

#include <string_view>
#include <unordered_set>
#include <map>

#include <gdeh0213b73.h>
#include <Fonts/FreeMono12pt7b.h>

#include "actor_runtime/native_actor.hpp"
#include "pubsub/publication.hpp"
#include "board_functions.hpp"

namespace uActor::ESP32::Notifications {
  class EPaperNotificationActor : public ActorRuntime::NativeActor {
    public:
    EPaperNotificationActor(ActorRuntime::ManagedNativeActor* actor_wrapper,
        std::string_view node_id, std::string_view actor_type,
        std::string_view instance_id);

    void receive (const PubSub::Publication& publication) override;

    void receive_notification(const PubSub::Publication& publication);

    void print_next();
    
    void cleanup();

    void print(std::string_view text);
    
    private:
    struct Notification {      
      Notification(std::string id, std::string text, uint32_t lifetime) : id(id), text(std::move(text)), lifetime(lifetime) {}      
      std::string id;
      std::string text;
      uint32_t lifetime;
    };

    std::map<std::string, Notification> notifications;
    std::string last_notification_id;
    std::string current_text;
    bool cleared = false;
  };

} // namespace uActor::ESP32::Notifications

#endif // COMPONENTS_EPAPER_ACTOR_INCLUDE_EPAPER_NOTIFICATION_ACTOR_HPP_