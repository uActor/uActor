#ifndef MAIN_INCLUDE_PUBLICATION_HPP_
#define MAIN_INCLUDE_PUBLICATION_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
class Publication {
 public:
  // TODO(raphaelhetzel) implement custom iterator to allow for a more efficient
  // implementation later
  using const_iterator =
      std::unordered_map<std::string, std::string>::const_iterator;
  const_iterator begin() const { return attributes->begin(); }
  const_iterator end() const { return attributes->end(); }

  Publication(std::string_view sender_node_id,
              std::string_view sender_actor_type,
              std::string_view sender_instance_id) {
    attributes = new std::unordered_map<std::string, std::string>();
    attributes->emplace(std::string("sender_node_id"),
                        std::string(sender_node_id));
    attributes->emplace(std::string("sender_instance_id"),
                        std::string(sender_instance_id));
    attributes->emplace(std::string("sender_actor_type"), sender_actor_type);
  }

  Publication() : attributes(nullptr) {}

  ~Publication() {
    if (attributes) {
      attributes->clear();
      delete attributes;
    }
  }

  // Required for using the freeRTOS queues
  void moved() { attributes = nullptr; }

  Publication(const Publication& old) {
    if (old.attributes != nullptr) {
      attributes =
          new std::unordered_map<std::string, std::string>(*(old.attributes));
    } else {
      attributes = nullptr;
    }
  }

  Publication& operator=(const Publication& old) {
    delete attributes;
    if (old.attributes != nullptr) {
      attributes =
          new std::unordered_map<std::string, std::string>(*(old.attributes));
    } else {
      attributes = nullptr;
    }
    return *this;
  }

  Publication(Publication&& old) : attributes(old.attributes) {
    old.attributes = nullptr;
  }

  Publication& operator=(Publication&& old) {
    delete attributes;
    attributes = old.attributes;
    old.attributes = nullptr;
    return *this;
  }

  bool has_attr(std::string_view name) const {
    return attributes->find(std::string(name)) != attributes->end();
  }

  std::optional<std::string_view> get_attr(std::string_view name) const {
    auto result = attributes->find(std::string(name));
    if (result != attributes->end()) {
      return std::string_view(result->second);
    }
    return std::nullopt;
  }
  void set_attr(std::string_view name, std::string_view value) {
    attributes->insert_or_assign(std::string(name), std::string(value));
  }

 private:
  // TODO(raphaelhetzel) It might be beneficial to use a
  // datastructure with less overhead (size+allocations) here.
  std::unordered_map<std::string, std::string>* attributes;
};
#endif  // MAIN_INCLUDE_PUBLICATION_HPP_
