#include "pubsub/publication.hpp"

#include <array>
#include <msgpack.hpp>

#include "support/logger.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

Publication::Publication(std::string_view publisher_node_id,
                         std::string_view publisher_actor_type,
                         std::string_view publisher_instance_id)
    : attributes(std::make_shared<Map>()), _shallow_copy(false) {
  attributes->set_attr("publisher_node_id", publisher_node_id);
  attributes->set_attr("publisher_instance_id", publisher_instance_id);
  attributes->set_attr("publisher_actor_type", publisher_actor_type);
}

Publication::Publication()
    : attributes(std::make_shared<Map>()), _shallow_copy(false) {}

Publication::Publication(size_t size_hint)
    : attributes(std::make_shared<Map>()), _shallow_copy(false) {}

Publication::Publication(const Publication& old) {
  if (old.attributes->has_map_attrs()) {
    // Deep copy required
    attributes = std::make_shared<Map>(*old.attributes);
    _shallow_copy = false;
  } else {
    // Shallow copy is sufficient
    attributes = old.attributes;
    _shallow_copy = true;
    // NOLINTNEXTLINE (cppcoreguidelines-pro-type-const-cast)
    const_cast<Publication&>(old)._shallow_copy = true;
  }
}

Publication& Publication::operator=(const Publication& old) {
  if (this == &old) {
    return *this;
  } else if (old.attributes->has_map_attrs()) {
    // Deep copy required
    attributes = std::make_shared<Map>(*old.attributes);
    _shallow_copy = false;
  } else {
    // Shallow copy is sufficient
    attributes = old.attributes;
    _shallow_copy = true;
    // NOLINTNEXTLINE (cppcoreguidelines-pro-type-const-cast)
    const_cast<Publication&>(old)._shallow_copy = true;
  }
  return *this;
}

Publication::Publication(Publication&& old)
    : attributes(std::move(old.attributes)), _shallow_copy(old._shallow_copy) {}

Publication& Publication::operator=(Publication&& old) {
  attributes = std::move(old.attributes);
  _shallow_copy = old._shallow_copy;
  return *this;
}

bool Publication::operator==(const Publication& other) {
  if (attributes != nullptr && other.attributes != nullptr) {
    return *root_handle() == *other.root_handle();
  }
  return false;
}

void pack_map_msgpack(msgpack::packer<VectorBuffer>* packer,
                      std::shared_ptr<PubSub::Publication::Map> map) {
  packer->pack_map(map->size());
  for (const auto& attr : *map) {
    if (std::holds_alternative<Publication::AString>(attr.second)) {
      packer->pack(std::string_view(attr.first));
      packer->pack(
          std::string_view(std::get<Publication::AString>(attr.second)));
    } else if (std::holds_alternative<int32_t>(attr.second)) {
      packer->pack(std::string_view(attr.first));
      packer->pack(std::get<int32_t>(attr.second));
    } else if (std::holds_alternative<float>(attr.second)) {
      packer->pack(std::string_view(attr.first));
      packer->pack(std::get<float>(attr.second));
    } else if (std::holds_alternative<std::shared_ptr<Publication::Map>>(
                   attr.second)) {
      auto map_data = std::get<std::shared_ptr<Publication::Map>>(attr.second);
      packer->pack(std::string_view(attr.first));
      pack_map_msgpack(packer, map_data);
    }
  }
}

std::shared_ptr<std::vector<char>> Publication::to_msg_pack() {
  try {
    if (!attributes) {
      Support::Logger::warning("PUBLICATION", "TO MSG PCK: MOVED");
      return std::make_shared<std::vector<char>>(4);
    }
    VectorBuffer buffer;
    msgpack::packer<VectorBuffer> packer(buffer);
    pack_map_msgpack(&packer, attributes);
    return std::move(buffer.fetch_buffer());
  } catch (std::bad_alloc& exception) {
    Support::Logger::error(
        "PUBLICATION", "TO_MSG_PACK",
        "Dropped outgoin message as the system is out of memory");
    return std::make_shared<std::vector<char>>(4);
  }
}

std::variant<std::monostate, std::string_view, int32_t, float,
             std::shared_ptr<Publication::Map>>
Publication::get_attr(std::string_view name) const {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Get: moved");
    return std::variant<std::monostate, std::string_view, int32_t, float,
                        std::shared_ptr<Publication::Map>>();
  }
  return attributes->get_attr(name);
}

std::optional<const std::string_view> Publication::get_str_attr(
    std::string_view name) const {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Get: moved");
    return std::nullopt;
  }
  return attributes->get_str_attr(name);
}

std::optional<int32_t> Publication::get_int_attr(std::string_view name) const {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Get: moved");
    return std::nullopt;
  }
  return attributes->get_int_attr(name);
}

std::optional<float> Publication::get_float_attr(std::string_view name) const {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Get: moved");
    return std::nullopt;
  }
  return attributes->get_float_attr(name);
}

std::optional<std::shared_ptr<Publication::Map>>
Publication::get_nested_component(std::string_view name) const {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Get: moved");
    return std::nullopt;
  }
  return attributes->get_nested_component(name);
}

void Publication::set_attr(std::string_view name, std::string_view value) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Get: moved");
    return;
  }
  if (is_shallow_copy()) {
    deep_copy();
  }
  attributes->set_attr(name, value);
}

void Publication::set_attr(std::string_view name, int32_t value) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Set: moved");
    return;
  }
  if (is_shallow_copy()) {
    deep_copy();
  }
  attributes->set_attr(name, value);
}

void Publication::set_attr(std::string_view name, float value) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Set: moved");
    return;
  }
  if (is_shallow_copy()) {
    deep_copy();
  }
  attributes->set_attr(name, value);
}

void Publication::set_attr(std::string_view name,
                           std::shared_ptr<Publication::Map> value) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Set: moved");
    return;
  }
  if (is_shallow_copy()) {
    deep_copy();
  }

  attributes->set_attr(name, value);
}

void Publication::erase_attr(std::string_view name) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "Erase: moved");
    return;
  }
  if (is_shallow_copy()) {
    deep_copy();
  }
  attributes->erase_attr(name);
}

// Publication::Map

Publication::Map::Map(const Map& old) {
  for (const auto& [key, value] : old.data) {
    if (std::holds_alternative<std::shared_ptr<Map>>(value)) {
      auto item = std::make_shared<Map>(*std::get<std::shared_ptr<Map>>(value));
      data[key] = std::move(item);
    } else if (std::holds_alternative<AString>(value)) {
      data[key] = std::get<AString>(value);
    } else if (std::holds_alternative<int32_t>(value)) {
      data[key] = std::get<int32_t>(value);
    } else if (std::holds_alternative<float>(value)) {
      data[key] = std::get<float>(value);
    }
  }
}

Publication::Map& Publication::Map::operator=(const Map& old) {
  for (const auto& [key, value] : old.data) {
    if (std::holds_alternative<std::shared_ptr<Map>>(value)) {
      auto item = std::make_shared<Map>(*std::get<std::shared_ptr<Map>>(value));
      data[key] = std::move(item);
    } else if (std::holds_alternative<AString>(value)) {
      data[key] = std::get<AString>(value);
    } else if (std::holds_alternative<int32_t>(value)) {
      data[key] = std::get<int32_t>(value);
    } else if (std::holds_alternative<float>(value)) {
      data[key] = std::get<float>(value);
    }
  }
  return *this;
}

bool Publication::Map::operator==(const Map& other) const {
  if (other.data.size() != data.size()) {
    return false;
  }
  for (const auto& [key, value] : data) {
    if (std::holds_alternative<AString>(value)) {
      if (other.get_str_attr(key) != std::get<AString>(value)) {
        return false;
      }
    } else if (std::holds_alternative<int32_t>(value)) {
      if (other.get_int_attr(key) != std::get<int32_t>(value)) {
        return false;
      }
    } else if (std::holds_alternative<float>(value)) {
      if (other.get_float_attr(key) != std::get<float>(value)) {
        return false;
      }
    } else if (std::holds_alternative<std::shared_ptr<Map>>(value)) {
      if (!(**other.get_nested_component(key) == **get_nested_component(key))) {
        return false;
      }
    }
  }
  return true;
}

std::variant<std::monostate, std::string_view, int32_t, float,
             std::shared_ptr<Publication::Map>>
Publication::Map::get_attr(std::string_view name) const {
  auto result = data.find(AString(name));
  if (result != data.end()) {
    if (std::holds_alternative<AString>(result->second)) {
      return std::string_view(std::get<AString>(result->second));
    } else if (std::holds_alternative<int32_t>(result->second)) {
      return std::get<int32_t>(result->second);
    } else if (std::holds_alternative<float>(result->second)) {
      return std::get<float>(result->second);
    } else if (std::holds_alternative<std::shared_ptr<Map>>(result->second)) {
      return std::get<std::shared_ptr<Map>>(result->second);
    }
  }
  return std::variant<std::monostate, std::string_view, int32_t, float,
                      std::shared_ptr<Publication::Map>>();
}

std::optional<const std::string_view> Publication::Map::get_str_attr(
    std::string_view name) const {
  auto result = data.find(AString(name, make_allocator<AString>()));
  if (result != data.end()) {
    if (std::holds_alternative<AString>(result->second)) {
      return std::string_view(std::get<AString>(result->second));
    }
  }
  return std::nullopt;
}

std::optional<int32_t> Publication::Map::get_int_attr(
    std::string_view name) const {
  auto result = data.find(AString(name, make_allocator<AString>()));
  if (result != data.end()) {
    if (std::holds_alternative<int32_t>(result->second)) {
      return std::get<int32_t>(result->second);
    }
  }
  return std::nullopt;
}

std::optional<float> Publication::Map::get_float_attr(
    std::string_view name) const {
  auto result = data.find(AString(name));
  if (result != data.end()) {
    if (std::holds_alternative<float>(result->second)) {
      return std::get<float>(result->second);
    }
  }
  return std::nullopt;
}

std::optional<std::shared_ptr<Publication::Map>>
Publication::Map::get_nested_component(std::string_view name) const {
  auto result = data.find(AString(name));
  if (result != data.end()) {
    if (std::holds_alternative<std::shared_ptr<Map>>(result->second)) {
      return std::get<std::shared_ptr<Map>>(result->second);
    }
  }
  return std::nullopt;
}

void Publication::Map::set_attr(std::string_view name, std::string_view value) {
  auto s = AString(name, make_allocator<AString>());
  auto v = AString(value, make_allocator<AString>());

  data.erase(s);
  data.emplace(s, v);

  // attributes->insert_or_assign(s, v);
}

void Publication::Map::set_attr(std::string_view name, int32_t value) {
  data.insert_or_assign(AString(name, make_allocator<AString>()), value);
}

void Publication::Map::set_attr(std::string_view name, float value) {
  data.insert_or_assign(AString(name, make_allocator<AString>()), value);
}

void Publication::Map::set_attr(std::string_view name,
                                std::shared_ptr<Publication::Map> value) {
  data.insert_or_assign(AString(name, make_allocator<AString>()), value);
}

void Publication::Map::erase_attr(std::string_view name) {
  data.erase(AString(name, make_allocator<AString>()));
}

std::string Publication::Map::to_string() const {
  std::string buffer = "{";
  for (const auto& [key, value] : data) {
    buffer += PubSub::Publication::AString(" ") + key + "=";
    if (std::holds_alternative<PubSub::Publication::AString>(value)) {
      buffer += std::get<PubSub::Publication::AString>(value);
    } else if (std::holds_alternative<int32_t>(value)) {
      buffer += std::to_string(std::get<int32_t>(value));
    } else if (std::holds_alternative<float>(value)) {
      buffer += std::to_string(std::get<float>(value));
    } else if (std::holds_alternative<
                   std::shared_ptr<PubSub::Publication::Map>>(value)) {
      buffer += std::get<std::shared_ptr<PubSub::Publication::Map>>(value)
                    ->to_string();
    }
  }
  return buffer + " }";
}

Publication Publication::unwrap() const {
  PubSub::Publication pub{*get_str_attr("publisher_node_id"),
                          *get_str_attr("publisher_actor_type"),
                          *get_str_attr("publisher_instance_id")};
  for (const auto& [key, value] : **get_nested_component("__wrapped")) {
    if (std::holds_alternative<AString>(value)) {
      pub.set_attr(key, std::get<AString>(value));
    } else if (std::holds_alternative<int32_t>(value)) {
      pub.set_attr(key, std::get<int32_t>(value));
    } else if (std::holds_alternative<float>(value)) {
      pub.set_attr(key, std::get<float>(value));
    } else if (std::holds_alternative<std::shared_ptr<Map>>(value)) {
      pub.set_attr(
          key, std::make_shared<Map>(*std::get<std::shared_ptr<Map>>(value)));
    }
  }
  pub.set_attr("__unwrapped", 1);
  return pub;
};

}  //  namespace uActor::PubSub
