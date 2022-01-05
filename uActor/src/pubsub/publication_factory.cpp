#include "pubsub/publication_factory.hpp"

// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>

#include <msgpack.hpp>

#include "support/logger.hpp"

namespace uActor::PubSub {

PublicationFactory::PublicationFactory()
    : storage(std::make_unique<Storage>()) {}

PublicationFactory::~PublicationFactory() = default;

PublicationFactory::PublicationFactory(PublicationFactory&&) = default;

PublicationFactory& PublicationFactory::operator=(PublicationFactory&&) =
    default;

struct PublicationFactory::Storage {
  msgpack::unpacker unpacker;
};

void PublicationFactory::write(const char* data, size_t size) {
  storage->unpacker.reserve_buffer(size);
  std::memcpy(storage->unpacker.buffer(), data, size);
  storage->unpacker.buffer_consumed(size);
}

void msgpack_to_map(std::shared_ptr<Publication::Map> handle,
                    msgpack::object_map map) {
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-union-access)
  for (const auto& value_pair : map) {
    if (value_pair.val.type == msgpack::type::object_type::STR) {
      handle->set_attr(value_pair.key.as<std::string>(),
                       value_pair.val.as<std::string>());
    } else if (value_pair.val.type == msgpack::type::object_type::FLOAT32 ||
               value_pair.val.type == msgpack::type::object_type::FLOAT64) {
      handle->set_attr(value_pair.key.as<std::string>(),
                       value_pair.val.as<float>());
    } else if (value_pair.val.type ==
                   msgpack::type::object_type::POSITIVE_INTEGER ||
               value_pair.val.type ==
                   msgpack::type::object_type::NEGATIVE_INTEGER) {
      handle->set_attr(value_pair.key.as<std::string>(),
                       value_pair.val.as<int32_t>());
    } else if (value_pair.val.type == msgpack::type::object_type::MAP) {
      auto element_handle = std::make_shared<Publication::Map>();
      uActor::PubSub::msgpack_to_map(element_handle, value_pair.val.via.map);
      handle->set_attr(value_pair.key.as<std::string>(), element_handle);
    }
  }
}

std::optional<Publication> PublicationFactory::build() {
  msgpack::unpacked result;
  if (storage->unpacker.next(result)) {
    msgpack::object oh(result.get());
    if (!(oh.type == msgpack::type::object_type::MAP)) {
      Support::Logger::warning("PUBLICATION-FACTORY", "Root type is not a map");
      return std::nullopt;
    }
    Publication p{};

    uActor::PubSub::msgpack_to_map(p.root_handle(), oh.via.map);

    return std::move(p);
  } else {
    Support::Logger::warning("PUBLICATION-FACTORY", "Empty msgpack data");
  }
  return std::nullopt;
}

}  // namespace uActor::PubSub
