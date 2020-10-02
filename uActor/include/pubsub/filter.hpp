#ifndef UACTOR_INCLUDE_PUBSUB_FILTER_HPP_
#define UACTOR_INCLUDE_PUBSUB_FILTER_HPP_

#include <list>
#include <optional>
#include <string>
#include <string_view>

#include "constraint.hpp"
#include "publication.hpp"

namespace uActor::PubSub {

class Subscription;
class Router;

class Filter {
 public:
  explicit Filter(std::initializer_list<Constraint> constraints);

  explicit Filter(std::list<Constraint>&& constraints);

  Filter() = default;

  void clear();

  bool matches(const Publication& publication) const;

  bool check_required(const Publication& publication) const;

  bool check_optionals(const Publication& publication) const;

  bool operator==(const Filter& other) const;

  std::string serialize() const;

  static std::optional<Filter> deserialize(std::string_view serialized);

 private:
  std::list<Constraint> required;
  std::list<Constraint> optional;
  friend Subscription;
  friend Router;
};
}  // namespace uActor::PubSub

#endif  //  UACTOR_INCLUDE_PUBSUB_FILTER_HPP_
