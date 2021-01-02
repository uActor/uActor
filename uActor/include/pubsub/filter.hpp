#pragma once

#include <functional>
#include <list>
#include <optional>
#include <string>
#include <string_view>

#include "constraint.hpp"
#include "publication.hpp"

namespace uActor::PubSub {

template <template <typename> typename Allocator,
          typename AllocatorConfiguration>
class Subscription;

class Router;

class Filter {
 public:
  Filter(std::initializer_list<Constraint> constraints);

  explicit Filter(std::list<Constraint>&& constraints);

  Filter() = default;

  void clear();

  [[nodiscard]] bool matches(const Publication& publication) const;

  [[nodiscard]] bool check_required(const Publication& publication) const;

  [[nodiscard]] bool check_optionals(const Publication& publication) const;

  [[nodiscard]] bool operator==(const Filter& other) const;

  [[nodiscard]] const std::list<Constraint>& required_constraints() const {
    return required;
  }

  [[nodiscard]] const std::list<Constraint>& optional_constraints() const {
    return optional;
  }

  [[nodiscard]] std::string serialize() const;

  static std::optional<Filter> deserialize(std::string_view serialized);

 private:
  std::list<Constraint> required;
  std::list<Constraint> optional;

  template <template <typename> typename Allocator, typename AllocatorConfig>
  friend class Subscription;
  friend Router;
};
}  // namespace uActor::PubSub
