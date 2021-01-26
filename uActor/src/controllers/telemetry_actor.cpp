#include "controllers/telemetry_actor.hpp"

namespace uActor::Controllers {

std::shared_mutex TelemetryData::mtx;
TelemetryData TelemetryData::instance;
std::function<void()> TelemetryActor::telemetry_fetch_hook = nullptr;

void TelemetryActor::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "init") {
    publish_trigger(60000);
  } else if (publication.get_str_attr("type") == "wakeup" &&
             publication.get_str_attr("wakeup_id") == "publish_trigger") {
    if (telemetry_fetch_hook) {
      telemetry_fetch_hook();
    }

    TelemetryData d = TelemetryData::replace_instance();
    try_publish_telemetry_datapoint(d);
    publish_trigger(60000);
  }
}

void TelemetryActor::publish_trigger(uint32_t delay) {
  enqueue_wakeup(delay, "publish_trigger");
}

void TelemetryActor::try_publish_telemetry_datapoint(
    const TelemetryData& data) {
  if (data.data.size() == 0) {
    return;
  }

  uActor::PubSub::Publication p;
  p.set_attr("type", "data_point");
  p.set_attr("measurement", "telemetry");

  std::string data_values;
  for (const auto& [key, value] : data.data) {
    data_values += key + ",";
    p.set_attr(key, value);
  }
  data_values.erase(data_values.size() - 1);
  p.set_attr("values", std::move(data_values));
  p.set_attr("tags", "node");
  p.set_attr("node", node_id());
  publish(std::move(p));
}

}  // namespace uActor::Controllers
