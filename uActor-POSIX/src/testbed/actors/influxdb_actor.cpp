#if CONFIG_UACTOR_ENABLE_INFLUXDB_ACTOR
#include "actors/influxdb_actor.hpp"

#include <InfluxDBException.h>

#include <memory>
#include <utility>

#include "support/logger.hpp"

namespace uActor::Database {

void InfluxDBActor::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "init") {
    subscribe(PubSub::Filter{PubSub::Constraint("type", "data_point")});
  } else if (publication.get_str_attr("type") == "data_point") {
    receive_data_point(publication);
  }
}

std::string InfluxDBActor::server_url =  // NOLINT
    "http://smart_office:smart_office_user@influxdb:8086?db=smart_office";

void InfluxDBActor::receive_data_point(const PubSub::Publication& publication) {
  if (publication.has_attr("measurement") && publication.has_attr("values")) {
    auto p =
        influxdb::Point(std::string(*publication.get_str_attr("measurement")));

    for (const auto value_key : Support::StringHelper::string_split(
             *publication.get_str_attr("values"))) {
      if (publication.has_attr(value_key)) {
        auto value = publication.get_attr(value_key);
        if (std::holds_alternative<std::string_view>(value)) {
          p.addField(value_key, std::string(std::get<std::string_view>(value)));
        } else if (std::holds_alternative<int32_t>(value)) {
          p.addField(value_key, std::get<int32_t>(value));
        } else if (std::holds_alternative<float>(value)) {
          p.addField(value_key, std::get<float>(value));
        }
      } else {
        return;
      }
    }

    if (publication.has_attr("tags")) {
      for (const auto tag_key : Support::StringHelper::string_split(
               *publication.get_str_attr("tags"))) {
        if (publication.has_attr(tag_key)) {
          auto value = *publication.get_str_attr(tag_key);
          p.addTag(tag_key, value);
        } else {
          return;
        }
      }
    }

    if (publication.has_attr("timestamp")) {
      p.setTimestamp(std::chrono::time_point<std::chrono::system_clock>(
          std::chrono::seconds(*publication.get_int_attr("timestamp"))));
    }

    try {
      std::unique_ptr<influxdb::InfluxDB> connection =
          influxdb::InfluxDBFactory::Get(server_url);
      connection->write(std::move(p));
    } catch (const ::influxdb::InfluxDBException&) {
      Support::Logger::error("INFLUXDB-ACTOR", "Write: Connection error");
    }
  }
}
}  // namespace uActor::Database
#endif
