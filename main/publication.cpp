#include "include/publication.hpp"

#include "msgpack.hpp"

std::string Publication::to_msg_pack() {
  msgpack::sbuffer sbuf{512ul};
  msgpack::packer<msgpack::sbuffer> packer(sbuf);
  packer.pack_map(attributes->size());
  for (const auto& attr : *attributes) {
    if (std::holds_alternative<std::string>(attr.second)) {
      packer.pack(attr.first);
      packer.pack(std::get<std::string>(attr.second));
    } else if (std::holds_alternative<int32_t>(attr.second)) {
      packer.pack(attr.first);
      packer.pack(std::get<int32_t>(attr.second));
    } else if (std::holds_alternative<float>(attr.second)) {
      packer.pack(attr.first);
      packer.pack(std::get<float>(attr.second));
    }
  }
  return std::string(sbuf.data(), sbuf.size());
}

std::optional<Publication> Publication::from_msg_pack(
    std::string_view message) {
  msgpack::object_handle message_object =
      msgpack::unpack(message.data(), message.size());
  if (!(message_object->type == msgpack::type::object_type::MAP)) {
    return std::nullopt;
  }
  Publication p{};
  for (const auto& value_pair : message_object->via.map) {
    if (value_pair.val.type == msgpack::type::object_type::STR) {
      p.set_attr(value_pair.key.as<std::string>(),
                 value_pair.val.as<std::string>());
    } else if (value_pair.val.type == msgpack::type::object_type::FLOAT32 ||
               value_pair.val.type == msgpack::type::object_type::FLOAT64) {
      p.set_attr(value_pair.key.as<std::string>(), value_pair.val.as<float>());
    } else if (value_pair.val.type ==
                   msgpack::type::object_type::POSITIVE_INTEGER ||
               value_pair.val.type ==
                   msgpack::type::object_type::NEGATIVE_INTEGER) {
      p.set_attr(value_pair.key.as<std::string>(),
                 value_pair.val.as<int32_t>());
    }
  }
  return std::move(p);
}
