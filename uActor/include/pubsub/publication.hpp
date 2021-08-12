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
#include "pubsub/vector_buffer.hpp"
#include "support/logger.hpp"
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

  class Map : std::enable_shared_from_this<Map> {
   public:
    using BinType = std::vector<char>;
    using VariantType =
        std::variant<AString, int32_t, float, BinType, std::shared_ptr<Map>>;
    // TODO(raphaelhetzel) It might be beneficial to use a
    // datastructure with less overhead (size+allocations) here.
    using DataType =
        std::unordered_map<AString, VariantType, Support::StringHash,
                           Support::StringEqual,
                           Allocator<std::pair<const AString, VariantType>>>;

    // TODO(raphaelhetzel) implement custom iterator to allow for a more
    // efficient implementation later
    using const_iterator = DataType::const_iterator;

    [[nodiscard]] const_iterator begin() const { return data.begin(); }

    [[nodiscard]] const_iterator end() const { return data.end(); }

    Map() = default;

    Map(const Map& old);
    Map& operator=(const Map& old);

    Map(Map&& old) = default;
    Map& operator=(Map&& old) = default;

    bool operator==(const Map& other) const;

    [[nodiscard]] bool has_attr(std::string_view name) const {
      return data.find(AString(name)) != data.end();
    }

    [[nodiscard]] std::variant<std::monostate, std::string_view, int32_t, float,
                               std::shared_ptr<Map>>
    get_attr(std::string_view name) const;

    [[nodiscard]] std::optional<const std::string_view> get_str_attr(
        std::string_view name) const;
    [[nodiscard]] std::optional<int32_t> get_int_attr(
        std::string_view name) const;
    [[nodiscard]] std::optional<float> get_float_attr(
        std::string_view name) const;
    [[nodiscard]] std::optional<std::shared_ptr<Map>> get_nested_component(
        std::string_view name) const;

    void set_attr(std::string_view name, std::string_view value);
    void set_attr(std::string_view name, int32_t value);
    void set_attr(std::string_view name, float value);
    void set_attr(std::string_view name, std::shared_ptr<Map> value);

    void erase_attr(std::string_view name);

    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] size_t size() const { return data.size(); }

   private:
    DataType data{};

    [[nodiscard]] bool has_map_attrs() {
      for (const auto& [key, value] : data) {
        if (std::holds_alternative<std::shared_ptr<Map>>(value)) {
          return true;
        }
      }
      return false;
    }

    friend Publication;
  };

  const std::shared_ptr<Map> root_handle() const { return attributes; }

  Map* root_ptr() { return attributes.get(); }

  using const_iterator = Map::const_iterator;
  [[nodiscard]] const_iterator begin() const { return attributes->begin(); }

  [[nodiscard]] const_iterator end() const { return attributes->end(); }

  Publication(std::string_view publisher_node_id,
              std::string_view publisher_actor_type,
              std::string_view publisher_instance_id);

  Publication();

  explicit Publication(size_t size_hint);

  ~Publication() = default;

  std::shared_ptr<std::vector<char>> to_msg_pack();

  // If possible (no nested attributes), this performs a shallow copy of the
  // underlying data that will only be copied if there is a write operation
  // (Copy on write)
  Publication(const Publication& old);

  // If possible (no nested attributes), this performs a shallow copy of the
  // underlying data that will only be copied if there is a write operation
  // (Copy on write)
  Publication& operator=(const Publication& old);

  Publication(Publication&& old);

  Publication& operator=(Publication&& old);

  bool operator==(const Publication& other);

  [[nodiscard]] bool has_attr(std::string_view name) const {
    return attributes->has_attr(name);
  }

  [[nodiscard]] std::variant<std::monostate, std::string_view, int32_t, float,
                             std::shared_ptr<Map>>
  get_attr(std::string_view name) const;

  [[nodiscard]] std::optional<const std::string_view> get_str_attr(
      std::string_view name) const;
  [[nodiscard]] std::optional<int32_t> get_int_attr(
      std::string_view name) const;
  [[nodiscard]] std::optional<float> get_float_attr(
      std::string_view name) const;
  [[nodiscard]] std::optional<std::shared_ptr<Map>> get_nested_component(
      std::string_view name) const;
  [[nodiscard]] std::optional<bin_type&> get_bin_attr(


  void set_attr(std::string_view name, std::string_view value);
  void set_attr(std::string_view name, int32_t value);
  void set_attr(std::string_view name, float value);
  void set_attr(std::string_view name, std::shared_ptr<Map> value);
  void set_attr(std::string_view name, bin_type value);

  void erase_attr(std::string_view name);

  [[nodiscard]] bool is_shallow_copy() const { return _shallow_copy; }

  void deep_copy() {
    attributes = std::make_shared<Map>(*attributes);
    _shallow_copy = false;
  }

  [[nodiscard]] size_t size() const { return attributes->size(); }

  [[nodiscard]] Publication unwrap() const;

 private:
  std::shared_ptr<Map> attributes;
  bool _shallow_copy;
};

}  //  namespace uActor::PubSub
