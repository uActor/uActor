#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "allocator_configuration.hpp"
#include "support/memory_manager.hpp"

namespace uActor::PubSub {

class Publication {
 public:
  template <typename U>
  using Allocator = PublicationAllocatorConfiguration::Allocator<U>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  template <typename U>
  constexpr static auto make_allocator =
      PublicationAllocatorConfiguration::make_allocator<U>;

  using variant_type = std::variant<AString, int32_t, float>;
  using InternalType =
      std::unordered_map<AString, variant_type, Support::StringHash,
                         Support::StringEqual,
                         Allocator<std::pair<const AString, variant_type>>>;

  // TODO(raphaelhetzel) implement custom iterator to allow for a more efficient
  // implementation later
  using const_iterator = InternalType::const_iterator;
  [[nodiscard]] const_iterator begin() const { return attributes->begin(); }
  [[nodiscard]] const_iterator end() const { return attributes->end(); }

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

  [[nodiscard]] bool has_attr(std::string_view name) const {
    return attributes->find(AString(name)) != attributes->end();
  }

  [[nodiscard]] std::variant<std::monostate, std::string_view, int32_t, float>
  get_attr(std::string_view name) const;

  [[nodiscard]] std::optional<const std::string_view> get_str_attr(
      std::string_view name) const;
  [[nodiscard]] std::optional<int32_t> get_int_attr(
      std::string_view name) const;
  [[nodiscard]] std::optional<float> get_float_attr(
      std::string_view name) const;

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
  // todo (raphaelhetzel) really required? It is never used
  uint32_t size = 0;
};

}  //  namespace uActor::PubSub
