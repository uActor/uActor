
#include "ble_actor.hpp"

#include <base64.h>

#include <cstring>

#include "pubsub/publication.hpp"
#include "pubsub/router.hpp"

namespace uActor::ESP32::BLE {

#if CONFIG_UACTOR_OPTIMIZATIONS_BLE_FILTER
std::list<BLEActor::InternalBLEFilter> BLEActor::active_filters;
#endif

void BLEActor::os_task(void* args) {
  auto actor = BLEActor();
  while (true) {
    auto result = actor.handle.receive(BoardFunctions::SLEEP_FOREVER);
    if (result) {
      actor.receive(std::move(result->publication));
    }
  }
}

BLEActor::BLEActor() : handle(PubSub::Router::get_instance().new_subscriber()) {
  ble_init();

  PubSub::Filter primary_filter{
      PubSub::Constraint(std::string("node_id"), BoardFunctions::NODE_ID),
      PubSub::Constraint(std::string("actor_type"), "core.io.ble"),
      PubSub::Constraint(std::string("instance_id"), "1")};
  handle.subscribe(primary_filter);

  PubSub::Filter ble_filter{PubSub::Constraint("type", "ble_advertisement")};
  handle.subscribe(ble_filter);

#if CONFIG_UACTOR_OPTIMIZATIONS_BLE_FILTER
  handle.subscribe(
      PubSub::Filter{PubSub::Constraint("type", "subscription_added"),
                     PubSub::Constraint("publisher_node_id",
                                        std::string(BoardFunctions::NODE_ID))});

  handle.subscribe(
      PubSub::Filter{PubSub::Constraint("type", "subscription_removed"),
                     PubSub::Constraint("publisher_node_id",
                                        std::string(BoardFunctions::NODE_ID))});
#endif

  PubSub::Publication p{BoardFunctions::NODE_ID, "core.io.ble", "1"};
  p.set_attr("type", "unmanaged_actor_update");
  p.set_attr("command", "register");
  p.set_attr("update_actor_type", "core.io.ble");
  p.set_attr("node_id", BoardFunctions::NODE_ID);
  PubSub::Router::get_instance().publish(std::move(p));
}

BLEActor::~BLEActor() {
  PubSub::Publication p{BoardFunctions::NODE_ID, "core.io.ble", "1"};
  p.set_attr("type", "unmanaged_actor_update");
  p.set_attr("command", "deregister");
  p.set_attr("update_actor_type", "core.io.ble");
  p.set_attr("node_id", BoardFunctions::NODE_ID);
  PubSub::Router::get_instance().publish(std::move(p));
}

void BLEActor::ble_init() {
  esp_nimble_hci_and_controller_init();
  nimble_port_init();

  // global configuration
  ble_hs_cfg.reset_cb = on_reset;
  ble_hs_cfg.sync_cb = on_sync;

  nimble_port_freertos_init(&host_task);
}

void BLEActor::receive(PubSub::Publication&& publication) {
  if (publication.get_str_attr("type") == "ble_advertisement") {
    handle_advertisement_receive(std::move(publication));
#if CONFIG_UACTOR_OPTIMIZATIONS_BLE_FILTER
  } else if (publication.get_str_attr("type") == "subscription_removed") {
    handle_subscription_added(std::move(publication));
  } else if (publication.get_str_attr("type") == "subscription_added") {
    handle_subscription_added(std::move(publication));
  }
#else
  }
#endif
}

void BLEActor::handle_advertisement_receive(PubSub::Publication&& publication) {
  std::map<uint8_t, std::string> attributes;
  for (uint8_t i = 0x01; i > 0x00; i++) {
    char attr_id_string[5];
    snprintf(attr_id_string, sizeof(attr_id_string), "0x%02X", i);
    if (publication.has_attr(attr_id_string)) {
      attributes.emplace(
          i, base64_decode(*publication.get_str_attr(attr_id_string)));
    }
  }
  advertise(std::move(attributes));
}

#if CONFIG_UACTOR_OPTIMIZATIONS_BLE_FILTER
void BLEActor::handle_subscription_added(PubSub::Publication&& publication) {
  if (!publication.has_attr("serialized_subscription")) {
    return;
  }

  auto deserialized = PubSub::Filter::deserialize(
      *publication.get_str_attr("serialized_subscription"));

  if (deserialized && filter_has_ble_type_constraint(*deserialized)) {
    if (std::find_if(
            active_filters.begin(), active_filters.end(),
            [&filter = std::as_const(*deserialized)](const auto& other) {
              return filter == other.filter();
            }) != active_filters.end()) {
      return;
    }

    InternalBLEFilter filter;
    for (const auto& c : deserialized->required_constraints()) {
      try {
        auto id = std::stoi(std::string(c.attribute()), 0, 0);
        if (id <= 0xFF && std::holds_alternative<std::string>(c.operand())) {
          filter.set_field(id,
                           base64_decode(std::get<std::string>(c.operand())),
                           c.predicate());
        }
      } catch (const std::exception&) {
      }
    }
    filter.filter(std::move(*deserialized));
    active_filters.push_front(std::move(filter));
  }
}

void BLEActor::handle_subscription_removed(PubSub::Publication&& publication) {
  if (!publication.has_attr("serialize_subscription")) {
    return;
  }
  auto deserialized = PubSub::Filter::deserialize(
      *publication.get_str_attr("serialized_subscription"));

  if (deserialized && filter_has_ble_type_constraint(*deserialized)) {
    auto filter_it = std::find_if(
        active_filters.begin(), active_filters.end(),
        [&other = std::as_const(*deserialized)](const auto& filter) {
          return filter.filter() == other;
        });
    if (filter_it != active_filters.end()) {
      active_filters.erase(filter_it);
    }
  }
}

bool BLEActor::filter_has_ble_type_constraint(const PubSub::Filter& filter) {
  auto is_ble_type_constraint = [](const auto& constraint) {
    return constraint.attribute() == "type" &&
           std::holds_alternative<std::string>(constraint.operand()) &&
           std::get<std::string>(constraint.operand()) == "ble_discovery";
  };

  return std::find_if(filter.required_constraints().begin(),
                      filter.required_constraints().end(),
                      is_ble_type_constraint) !=
         filter.required_constraints().end();
}
#endif

void BLEActor::host_task(void* param) {
  // runs until explicitly stopped
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void BLEActor::on_reset(int reason) {
  printf("Called unimplemented on_reset callback!\n");
}

void BLEActor::on_sync(void) {
  set_random_addr();
  assert(ble_hs_util_ensure_addr(1) == 0);

  scan();
}

void BLEActor::set_random_addr() {
  ble_addr_t addr;
  assert(ble_hs_id_gen_rnd(1, &addr) == 0);
  assert(ble_hs_id_set_rnd(addr.val) == 0);
}

void BLEActor::advertise(std::map<uint8_t, std::string>&& attributes) {
  ble_gap_adv_params adv_params{
      BLE_GAP_CONN_MODE_NON,  // advertising mode
      BLE_GAP_DISC_MODE_NON,  // discoverable mode
      0,                      // itvl_min
      0,                      // itvl_max
      0,                      // channel map
      0,                      // filter policy
      0                       // high_duty_cycle_flag
  };

  if (ble_gap_adv_active()) {
    auto stop_rc = ble_gap_adv_stop();
    assert(stop_rc == 0 || stop_rc == BLE_HS_EALREADY);
  }

  uint8_t own_addr_type;
  assert(ble_hs_id_infer_auto(0, &own_addr_type) == 0);

  std::string advertisement = "";
  for (auto attribute : attributes) {
    auto result = serialize_attribute(attribute.first, attribute.second);
    if (result) {
      advertisement.append(*result);
    }
  }

  while (true) {
    auto start_rc = ble_gap_adv_set_data(
        reinterpret_cast<const uint8_t*>(advertisement.c_str()),
        advertisement.size());
    if (start_rc == BLE_HS_EBUSY) {
      auto stop_rc = ble_gap_adv_stop();
      assert(stop_rc == 0 || stop_rc == BLE_HS_EALREADY);
    } else {
      assert(start_rc == 0);
      break;
    }
  }

  assert(!ble_gap_adv_start(own_addr_type,   // infer own address
                            NULL,            // no address needed
                            BLE_HS_FOREVER,  // duration
                            &adv_params,     // params
                            NULL,            // callback
                            NULL));          // callback arguments
}

void BLEActor::scan(void) {
  uint8_t own_addr_type;
  assert(ble_hs_id_infer_auto(1, &own_addr_type) == 0);

  ble_gap_disc_params disc_params{
      0,  // itvl
      0,  // window
      0,  // filter_policy,
      0,  // limited
      0,  // passive
      0   // filter_duplicated
  };

  assert(ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      &discovery_callback, NULL) == 0);
}

int BLEActor::discovery_callback(ble_gap_event* event, void* arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_DISC:
      handle_discovery_event(event);
      return 0;
    default:
      printf("Unhandles event: %d\n", event->type);
      return 0;
  }
}

std::optional<std::string> BLEActor::serialize_attribute(
    uint8_t key, std::string attribute) {
  if (attribute.size() > 31) {
    return std::nullopt;
  }
  char buffer[31];

  buffer[0] = attribute.size() + 1;
  buffer[1] = key;
  memcpy(reinterpret_cast<void*>(&buffer[2]), attribute.data(),
         attribute.size());

  return std::string(buffer, attribute.size() + 2);
}

void BLEActor::handle_discovery_event(ble_gap_event* event) {
#if CONFIG_UACTOR_OPTIMIZATIONS_BLE_FILTER

  bool interest = false;
  for (const auto& filter : active_filters) {
    if (filter.sub_matches(event->disc.data, event->disc.length_data)) {
      interest = true;
      break;
    }
  }
  if (!interest) {
    return;
  }
#endif

  PubSub::Publication publication(BoardFunctions::NODE_ID, "ble_observer", "1");
  publication.set_attr("type", "ble_discovery");
  publication.set_attr("rssi", event->disc.rssi);
  publication.set_attr(
      "address", base64_encode(std::string(
                     reinterpret_cast<const char*>(event->disc.addr.val), 6)));

  std::string type;
  switch (event->disc.addr.type) {
    case BLE_ADDR_PUBLIC:
      type = "PUBLIC";
      break;
    case BLE_ADDR_RANDOM:
      if (BLE_ADDR_IS_RPA(&event->disc.addr)) {
        type = "PRIVATE_RESOLVEABLE";
      } else if (BLE_ADDR_IS_NRPA(&event->disc.addr)) {
        type = "PRIVATE_NON_RESOLVEABLE";
      } else {
        type = "PRIVATE_STATIC";
      }
      break;
    default:
      type = "OTHER";
      break;
  }
  publication.set_attr("address_type", type);

  walk_ble_packet(event->disc.data, event->disc.length_data,
                  [&publication = publication](uint8_t attribute_id,
                                               const std::string& value) {
                    char attr_id_string[5];
                    snprintf(attr_id_string, sizeof(attr_id_string), "0x%02X",
                             attribute_id);
                    publication.set_attr(std::string(attr_id_string),
                                         base64_encode(value, false));
                  });

  PubSub::Router::get_instance().publish(std::move(publication));
}
};  // namespace uActor::ESP32::BLE
