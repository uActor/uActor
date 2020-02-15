#include "pubsub/publication.hpp"

#include "msgpack.hpp"

namespace uActor::PubSub {

Publication::Publication(std::string_view publisher_node_id,
                         std::string_view publisher_actor_type,
                         std::string_view publisher_instance_id) {
  attributes = new std::unordered_map<std::string, variant_type>();
  attributes->emplace(std::string("publisher_node_id"),
                      std::string(publisher_node_id));
  attributes->emplace(std::string("publisher_instance_id"),
                      std::string(publisher_instance_id));
  attributes->emplace(std::string("publisher_actor_type"),
                      std::string(publisher_actor_type));
}

Publication::Publication()
    : attributes(new std::unordered_map<std::string, variant_type>()) {}

Publication::~Publication() {
  if (attributes) {
    attributes->clear();
    delete attributes;
  }
}

Publication::Publication(const Publication& old) {
  if (old.attributes != nullptr) {
    attributes =
        new std::unordered_map<std::string, variant_type>(*(old.attributes));
  } else {
    attributes = nullptr;
  }
}

Publication& Publication::operator=(const Publication& old) {
  delete attributes;
  if (old.attributes != nullptr) {
    attributes =
        new std::unordered_map<std::string, variant_type>(*(old.attributes));
  } else {
    attributes = nullptr;
  }
  return *this;
}

Publication::Publication(Publication&& old) : attributes(old.attributes) {
  old.attributes = nullptr;
}

Publication& Publication::operator=(Publication&& old) {
  delete attributes;
  attributes = old.attributes;
  old.attributes = nullptr;
  return *this;
}

bool Publication::operator==(const Publication& other) {
  if (attributes != nullptr && other.attributes != nullptr) {
    return *attributes == *other.attributes;
  }
  return false;
}

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

std::variant<std::monostate, std::string_view, int32_t, float>
Publication::get_attr(std::string_view name) const {
  auto result = attributes->find(std::string(name));
  if (result != attributes->end()) {
    if (std::holds_alternative<std::string>(result->second)) {
      return std::string_view(std::get<std::string>(result->second));
    } else if (std::holds_alternative<int32_t>(result->second)) {
      return std::get<int32_t>(result->second);
    } else if (std::holds_alternative<float>(result->second)) {
      return std::get<float>(result->second);
    }
  }
  return std::variant<std::monostate, std::string_view, int32_t, float>();
}

std::optional<const std::string_view> Publication::get_str_attr(
    std::string_view name) const {
  auto result = attributes->find(std::string(name));
  if (result != attributes->end()) {
    if (std::holds_alternative<std::string>(result->second)) {
      return std::string_view(std::get<std::string>(result->second));
    }
  }
  return std::nullopt;
}

std::optional<int32_t> Publication::get_int_attr(std::string_view name) const {
  auto result = attributes->find(std::string(name));
  if (result != attributes->end()) {
    if (std::holds_alternative<int32_t>(result->second)) {
      return std::get<int32_t>(result->second);
    }
  }
  return std::nullopt;
}

}  //  namespace uActor::PubSub
