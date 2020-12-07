#include "pubsub/publication_factory.hpp"

// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>

#include <msgpack.hpp>

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

std::optional<Publication> PublicationFactory::build() {
  msgpack::unpacked result;
  if (storage->unpacker.next(result)) {
    msgpack::object oh(result.get());
    if (!(oh.type == msgpack::type::object_type::MAP)) {
      printf("not a map\n");
      return std::nullopt;
    }
    Publication p{};
    for (const auto& value_pair : oh.via.map) {
      if (value_pair.val.type == msgpack::type::object_type::STR) {
        p.set_attr(value_pair.key.as<std::string>(),
                   value_pair.val.as<std::string>());
      } else if (value_pair.val.type == msgpack::type::object_type::FLOAT32 ||
                 value_pair.val.type == msgpack::type::object_type::FLOAT64) {
        p.set_attr(value_pair.key.as<std::string>(),
                   value_pair.val.as<float>());
      } else if (value_pair.val.type ==
                     msgpack::type::object_type::POSITIVE_INTEGER ||
                 value_pair.val.type ==
                     msgpack::type::object_type::NEGATIVE_INTEGER) {
        p.set_attr(value_pair.key.as<std::string>(),
                   value_pair.val.as<int32_t>());
      }
    }
    return std::move(p);
  } else {
    printf("else\n");
  }
  return std::nullopt;
}

}  // namespace uActor::PubSub
