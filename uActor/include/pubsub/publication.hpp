#ifndef UACTOR_INCLUDE_PUBSUB_PUBLICATION_HPP_
#define UACTOR_INCLUDE_PUBSUB_PUBLICATION_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace uActor::PubSub {

class Publication {
 public:
  using variant_type = std::variant<std::string, int32_t, float>;

  // TODO(raphaelhetzel) implement custom iterator to allow for a more efficient
  // implementation later
  using const_iterator =
      std::unordered_map<std::string, variant_type>::const_iterator;
  const_iterator begin() const { return attributes->begin(); }
  const_iterator end() const { return attributes->end(); }

  Publication(std::string_view publisher_node_id,
              std::string_view publisher_actor_type,
              std::string_view publisher_instance_id);

  Publication();

  explicit Publication(size_t size_hint);

  ~Publication();

  std::string to_msg_pack();

  static std::optional<Publication> from_msg_pack(std::string_view message);

  // Required for using the freeRTOS queues
  void moved() { attributes = nullptr; }

  Publication(const Publication& old);

  Publication& operator=(const Publication& old);

  Publication(Publication&& old);

  Publication& operator=(Publication&& old);

  bool operator==(const Publication& other);

  bool has_attr(std::string_view name) const {
    return attributes->find(std::string(name)) != attributes->end();
  }

  std::variant<std::monostate, std::string_view, int32_t, float> get_attr(
      std::string_view name) const;

  std::optional<const std::string_view> get_str_attr(
      std::string_view name) const;
  std::optional<int32_t> get_int_attr(std::string_view name) const;

  void set_attr(std::string_view name, std::string_view value) {
    attributes->insert_or_assign(std::string(name), std::string(value));
  }

  void set_attr(std::string_view name, int32_t value) {
    attributes->insert_or_assign(std::string(name), value);
  }

  void set_attr(std::string_view name, float value) {
    attributes->insert_or_assign(std::string(name), value);
  }

  void erase_attr(std::string_view name) {
    attributes->erase(std::string(name));
  }

 private:
  // TODO(raphaelhetzel) It might be beneficial to use a
  // datastructure with less overhead (size+allocations) here.
  std::unordered_map<std::string, variant_type>* attributes;
};

}  //  namespace uActor::PubSub

#endif  // UACTOR_INCLUDE_PUBSUB_PUBLICATION_HPP_
