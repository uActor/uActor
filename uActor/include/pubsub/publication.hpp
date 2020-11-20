#ifndef UACTOR_INCLUDE_PUBSUB_PUBLICATION_HPP_
#define UACTOR_INCLUDE_PUBSUB_PUBLICATION_HPP_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace uActor::PubSub {

class Publication {
 public:
  using variant_type = std::variant<std::string, int32_t, float>;
  using InternalType = std::unordered_map<std::string, variant_type>;

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

  std::shared_ptr<std::vector<char>> to_msg_pack();

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
  std::optional<float> get_float_attr(std::string_view name) const;

  void set_attr(std::string_view name, std::string_view value);

  void set_attr(std::string_view name, int32_t value);

  void set_attr(std::string_view name, float value);

  void erase_attr(std::string_view name);

 private:
  // TODO(raphaelhetzel) It might be beneficial to use a
  // datastructure with less overhead (size+allocations) here.
  std::shared_ptr<InternalType> attributes;
  bool shallow_copy;

 public:
  uint32_t size;
};

}  //  namespace uActor::PubSub

#endif  // UACTOR_INCLUDE_PUBSUB_PUBLICATION_HPP_
