#include "epaper_actor.hpp"

EpdSpi io;
Gdeh0213b73 display(io);

namespace uActor::ESP32::Notifications {

EPaperNotificationActor::EPaperNotificationActor(
    ActorRuntime::ManagedNativeActor* actor_wrapper, std::string_view node_id,
    std::string_view actor_type, std::string_view instance_id)
    : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {
  display.init(false);
  display.setRotation(1);
  display.fillScreen(EPD_WHITE);
  display.update();
}

void EPaperNotificationActor::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "notification") {
    receive_notification(publication);
  } else if (publication.get_str_attr("type") == "notification_cancelation") {
    receive_notification_cancelation(publication);
  } else if (publication.get_str_attr("type") == "init") {
    init();
  } else if (publication.get_str_attr("command") == "cleanup") {
    cleanup();
    send_cleanup_trigger();
  } else if (publication.get_str_attr("command") == "scroll_notifications") {
    print_next();
  } else if (publication.get_str_attr("type") == "exit") {
    exit();
  }
}

void EPaperNotificationActor::receive_notification(
    const PubSub::Publication& publication) {
  if (publication.has_attr("notification_id") &&
      publication.has_attr("notification_lifetime") &&
      publication.has_attr("notification_text")) {
    auto id = std::string(*publication.get_str_attr("notification_id"));
    auto text = std::string(*publication.get_str_attr("notification_text"));
    auto lifetime = static_cast<uint32_t>(
        *publication.get_int_attr("notification_lifetime"));

    if (lifetime > 0) {
      lifetime += now();
    }

    auto [it, inserted] =
        notifications.try_emplace(id, Notification(id, text, lifetime));
    if (!inserted) {
      it->second.lifetime = std::max(lifetime, it->second.lifetime);
      it->second.text = text;
    }

    if (inserted) {
      auto new_state = current_state;
      new_state.number_of_notifications = notifications.size();
      new_state.notification_text = text;
      new_state.notification_id = id;
      update(std::move(new_state));
    } else if (current_state.notification_id == id) {
      auto new_state = current_state;
      new_state.notification_text = text;
      update(std::move(new_state));
    }
  }
}

void EPaperNotificationActor::init() {
  subscribe(
      PubSub::Filter{PubSub::Constraint("command", "scroll_notifications"),
                     PubSub::Constraint("node_id", this->node_id().data())});
  subscribe(PubSub::Filter{PubSub::Constraint("type", "notification")});
  subscribe(
      PubSub::Filter{PubSub::Constraint("type", "notification_cancelation"),
                     PubSub::Constraint("node_id", this->node_id().data())});

  PubSub::Publication register_actor_type;
  register_actor_type.set_attr("type", "unmanaged_actor_update");
  register_actor_type.set_attr("command", "register");
  register_actor_type.set_attr("update_actor_type", "core.notification_center");
  register_actor_type.set_attr("node_id", this->node_id().data());
  publish(std::move(register_actor_type));

  send_cleanup_trigger();
  update(State("", std::string(node_id()), "uActor", notifications.size()),
         true);
}

void EPaperNotificationActor::exit() {
  PubSub::Publication deregister_actor_type;
  deregister_actor_type.set_attr("type", "unmanaged_actor_update");
  deregister_actor_type.set_attr("command", "deregister");
  deregister_actor_type.set_attr("update_actor_type",
                                 "core.notification_center");
  deregister_actor_type.set_attr("node_id", node_id().data());
  publish(std::move(deregister_actor_type));
}

void EPaperNotificationActor::receive_notification_cancelation(
    const PubSub::Publication& publication) {
  if (publication.has_attr("notification_id")) {
    auto id = *publication.get_str_attr("notification_id");
    size_t removed = notifications.erase(std::string(id));
    if (removed > 0) {
      auto new_state = current_state;
      new_state.number_of_notifications = notifications.size();
      if (id == current_state.notification_id) {
        print_next(std::move(new_state));
      } else {
        update(std::move(new_state));
      }
    }
  }
}

void EPaperNotificationActor::print_next(std::optional<State> new_state_input) {
  auto new_state =
      new_state_input ? std::move(*new_state_input) : current_state;
  // Wrap around
  auto it = notifications.find(current_state.notification_id);
  if (it == notifications.end() || ++it == notifications.end()) {
    it = notifications.begin();
  }

  if (it != notifications.end()) {
    new_state.notification_id = it->second.id;
    new_state.notification_text = it->second.text;
  } else {
    new_state.notification_id = "";
    new_state.notification_text = "";
  }
  update(std::move(new_state));
}

void EPaperNotificationActor::cleanup(std::optional<State> new_state_input) {
  auto new_state =
      new_state_input ? std::move(*new_state_input) : current_state;

  std::unordered_set<std::string> to_delete;
  for (const auto& [id, notification] : notifications) {
    if (notification.lifetime > 0 && notification.lifetime < now()) {
      to_delete.insert(id);
    }
  }
  for (const auto& id : to_delete) {
    notifications.erase(id);
  }

  if (to_delete.size() > 0) {
    new_state.number_of_notifications = notifications.size();
    if (to_delete.find(current_state.notification_id) != to_delete.end()) {
      print_next(std::move(new_state));
    } else {
      update(std::move(new_state));
    }
  }
}

void EPaperNotificationActor::send_cleanup_trigger() {
  PubSub::Publication retrigger(node_id(), actor_type(), instance_id());
  retrigger.set_attr("actor_type", actor_type());
  retrigger.set_attr("node_id", node_id());
  retrigger.set_attr("instance_id", instance_id());
  retrigger.set_attr("command", "cleanup");
  delayed_publish(std::move(retrigger), 10000);
}

void EPaperNotificationActor::update(State&& new_state, bool force) {
  if (new_state == current_state && !force) {
    return;
  } else {
    current_state = std::move(new_state);
  }

  display.fillScreen(EPD_WHITE);
  display.setCursor(0, 14);
  // This is seems to be a bug in the underlying library
  display.setTextColor(EPD_WHITE);

  display.setFont(&FreeMono12pt7b);
  display.println(current_state.node_id + " - " +
                  std::to_string(current_state.number_of_notifications));

  for (int i = 0; i < 250; i++) {
    display.drawPixel(i, 22, 1);
  }

  display.println(current_state.notification_text);
  display.update();
}
}  // namespace uActor::ESP32::Notifications
