#include "pubsub/publication.hpp"

#include <array>
#include <msgpack.hpp>

#include "pubsub/vector_buffer.hpp"
#include "support/logger.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

Publication::Publication(std::string_view publisher_node_id,
                         std::string_view publisher_actor_type,
                         std::string_view publisher_instance_id)
    : attributes(std::make_shared<InternalType>(
          make_allocator<InternalType::value_type>())),
      shallow_copy(false) {
  attributes->emplace(AString("publisher_node_id", make_allocator<AString>()),
                      AString(publisher_node_id, make_allocator<AString>()));
  attributes->emplace(
      AString("publisher_instance_id", make_allocator<AString>()),
      AString(publisher_instance_id, make_allocator<AString>()));
  attributes->emplace(
      AString("publisher_actor_type", make_allocator<AString>()),
      AString(publisher_actor_type, make_allocator<AString>()));
}

Publication::Publication()
    : attributes(std::make_shared<InternalType>(
          make_allocator<InternalType::value_type>())),
      shallow_copy(false) {}

Publication::Publication(size_t size_hint)
    : attributes(std::make_shared<InternalType>(
          size_hint, make_allocator<InternalType::value_type>())),
      shallow_copy(false) {}

Publication::~Publication() = default;

Publication::Publication(const Publication& old)
    : attributes(old.attributes), shallow_copy(true) {
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-const-cast)
  const_cast<Publication&>(old).shallow_copy = true;
}

Publication& Publication::operator=(const Publication& old) {
  if (this == &old) {
    return *this;
  }
  attributes = old.attributes;
  shallow_copy = true;
  const_cast<Publication&>(old).shallow_copy = true;
  return *this;
}

Publication::Publication(Publication&& old)
    : attributes(std::move(old.attributes)), shallow_copy(old.shallow_copy) {}

Publication& Publication::operator=(Publication&& old) {
  attributes = std::move(old.attributes);
  shallow_copy = old.shallow_copy;
  return *this;
}

bool Publication::operator==(const Publication& other) {
  if (attributes != nullptr && other.attributes != nullptr) {
    return *attributes == *other.attributes;
  }
  return false;
}

std::shared_ptr<std::vector<char>> Publication::to_msg_pack() {
  try {
    if (!attributes) {
      Support::Logger::warning("PUBLICATION", "TO_MP", "MOVED");
      return std::make_shared<std::vector<char>>(4);
    }
    VectorBuffer buffer;
    msgpack::packer<VectorBuffer> packer(buffer);
    packer.pack_map(attributes->size());
    for (const auto& attr : *attributes) {
      if (std::holds_alternative<AString>(attr.second)) {
        packer.pack(std::string_view(attr.first));
        packer.pack(std::string_view(std::get<AString>(attr.second)));
      } else if (std::holds_alternative<int32_t>(attr.second)) {
        packer.pack(std::string_view(attr.first));
        packer.pack(std::get<int32_t>(attr.second));
      } else if (std::holds_alternative<float>(attr.second)) {
        packer.pack(std::string_view(attr.first));
        packer.pack(std::get<float>(attr.second));
      }
    }
    return std::move(buffer.fetch_buffer());
  } catch (std::bad_alloc& exception) {
    Support::Logger::error(
        "PUBLICATION", "TO_MSG_PACK",
        "Dropped outgoin message as the system is out of memory");
    return std::make_shared<std::vector<char>>(4);
  }
}

std::variant<std::monostate, std::string_view, int32_t, float>
Publication::get_attr(std::string_view name) const {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "GET", "MOVED");
    return std::variant<std::monostate, std::string_view, int32_t, float>();
  }
  auto result = attributes->find(AString(name));
  if (result != attributes->end()) {
    if (std::holds_alternative<AString>(result->second)) {
      return std::string_view(std::get<AString>(result->second));
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
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "GET", "MOVED");
    return std::nullopt;
  }
  auto result = attributes->find(AString(name, make_allocator<AString>()));
  if (result != attributes->end()) {
    if (std::holds_alternative<AString>(result->second)) {
      return std::string_view(std::get<AString>(result->second));
    }
  }
  return std::nullopt;
}

std::optional<int32_t> Publication::get_int_attr(std::string_view name) const {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "GET", "MOVED");
    return std::nullopt;
  }
  auto result = attributes->find(AString(name, make_allocator<AString>()));
  if (result != attributes->end()) {
    if (std::holds_alternative<int32_t>(result->second)) {
      return std::get<int32_t>(result->second);
    }
  }
  return std::nullopt;
}

std::optional<float> Publication::get_float_attr(std::string_view name) const {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "GET", "MOVED");
    return std::nullopt;
  }
  auto result = attributes->find(AString(name));
  if (result != attributes->end()) {
    if (std::holds_alternative<float>(result->second)) {
      return std::get<float>(result->second);
    }
  }
  return std::nullopt;
}

void Publication::set_attr(std::string_view name, std::string_view value) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "SET", "MOVED");
    return;
  }
  if (shallow_copy) {
    attributes = std::make_shared<InternalType>(
        *attributes, make_allocator<InternalType::value_type>());
    shallow_copy = false;
  }
  auto s = AString(name, make_allocator<AString>());
  auto v = AString(value, make_allocator<AString>());
  attributes->erase(s);
  attributes->emplace(s, v);
  // attributes->insert_or_assign(s, v);
}

void Publication::set_attr(std::string_view name, int32_t value) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "SET", "MOVED");
    return;
  }
  if (shallow_copy) {
    attributes = std::make_shared<InternalType>(
        *attributes, make_allocator<InternalType::value_type>());
    shallow_copy = false;
  }
  attributes->insert_or_assign(AString(name, make_allocator<AString>()), value);
}

void Publication::set_attr(std::string_view name, float value) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "SET", "MOVED");
    return;
  }
  if (shallow_copy) {
    attributes = std::make_shared<InternalType>(
        *attributes, make_allocator<InternalType::value_type>());
    shallow_copy = false;
  }
  attributes->insert_or_assign(AString(name, make_allocator<AString>()), value);
}

void Publication::erase_attr(std::string_view name) {
  if (!attributes) {
    Support::Logger::warning("PUBLICATION", "ERASE", "MOVED");
    return;
  }
  if (shallow_copy) {
    attributes = std::make_shared<InternalType>(
        *attributes, make_allocator<InternalType::value_type>());
    shallow_copy = false;
  }
  attributes->erase(AString(name, make_allocator<AString>()));
}

}  //  namespace uActor::PubSub
