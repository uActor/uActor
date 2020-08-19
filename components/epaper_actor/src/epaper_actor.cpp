#include "epaper_actor.hpp"


EpdSpi io;
Gdeh0213b73 display(io);

namespace uActor::ESP32::Notifications {

  EPaperNotificationActor::EPaperNotificationActor(ActorRuntime::ManagedNativeActor* actor_wrapper,
      std::string_view node_id, std::string_view actor_type,
      std::string_view instance_id) : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {
  subscribe(PubSub::Filter{PubSub::Constraint("command", "print")});
  subscribe(PubSub::Filter{PubSub::Constraint("type", "notification")});
  display.init(false);
  display.setRotation(1);
  display.fillScreen(EPD_WHITE);
  display.update();
}


void EPaperNotificationActor::receive (const PubSub::Publication& publication) {
    if(publication.get_str_attr("command") == "print") {
      print(*publication.get_str_attr("text"));
    } else if(publication.get_str_attr("type") == "notification") {
      receive_notification(publication);
    } else if(publication.get_str_attr("type") == "init" || publication.get_str_attr("command") == "scroll_notifications") {
      cleanup();
      print_next();
      PubSub::Publication retrigger(node_id(), actor_type(), instance_id());
      retrigger.set_attr("actor_type", actor_type());
      retrigger.set_attr("node_id", node_id());
      retrigger.set_attr("instance_id", instance_id());
      retrigger.set_attr("command", "scroll_notifications");
      delayed_publish(std::move(retrigger), 10000);
    }
}

void EPaperNotificationActor::receive_notification(const PubSub::Publication& publication) {
  if(
    publication.has_attr("notification_id") &&
    publication.has_attr("notification_lifetime") &&
    publication.has_attr("notification_text") 
  ) {
    auto id = std::string(*publication.get_str_attr("notification_id"));
    auto text = std::string(*publication.get_str_attr("notification_text"));
    auto lifetime = static_cast<uint32_t>(*publication.get_int_attr("notification_lifetime"));

    if(lifetime > 0) {
      lifetime += now();
    }

    auto [it, inserted] = notifications.try_emplace(id, Notification(id, std::move(text), lifetime));
    if(!inserted) {
      it->second.lifetime = std::max(lifetime, it->second.lifetime);
    }
  }
}

void EPaperNotificationActor::print_next() {
  Notification next_message("1", "", 0);
  bool found = false;
  
  if(!last_notification_id.empty()) {
    auto it = notifications.find(last_notification_id);
    if(++it != notifications.end()) {
      next_message = it->second;
      found = true;
    }
  }
  
  if(!found) {
    auto it = notifications.begin();
    if(it != notifications.end()) {
      next_message = it->second;
      found = true;
    }
  }

  if(found) {
    last_notification_id = next_message.id;
    print(next_message.text);
    cleared = false;
  } else if(!cleared) {
    print("");
    cleared = true;
  }
}

void EPaperNotificationActor::cleanup() {
  std::unordered_set<std::string> to_delete;
  for(const auto& [id, notification] : notifications) {
    if(notification.lifetime > 0 && notification.lifetime < now()) {
      to_delete.insert(id);
      if(id == last_notification_id) {
        last_notification_id = "";
      }
    }
  }
  for(const auto& id : to_delete) {
    notifications.erase(id);
  }
}

void EPaperNotificationActor::print(std::string_view message) {
  
  if(message == current_text) {
    return;
  } else {
    current_text = std::string(message);  
  }
  
  display.fillScreen(EPD_WHITE);
  display.setCursor(0,14);
  display.setTextColor(EPD_WHITE);

  for(int i = 0; i < 250; i++) {
      display.drawPixel(i, 20, 1); 
  }

  display.setFont(&FreeMono12pt7b);
  display.println((std::string("uActor - ") + uActor::BoardFunctions::NODE_ID).c_str());

  if(message.length() > 0) {
    display.println(message.data());
  }
  display.update();
}
}