#pragma once

#include <string>
#include <utility>

#include "pubsub/publication.hpp"
#include "runtime_allocator_configuration.hpp"

namespace uActor::ActorRuntime {
template <template <typename> typename Allocator>
class ReplyContext {
 public:
  using allocator_type = Allocator<ReplyContext>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  template <typename PAllocator = allocator_type>
  explicit ReplyContext(
      const PAllocator& allocator =
          RuntimeAllocatorConfiguration::make_allocator<ReplyContext>)
      : _node_id(allocator), _instance_id(allocator), _actor_type(allocator) {}

  template <typename PAllocator = allocator_type>
  ReplyContext(const ReplyContext& other,
               const PAllocator& allocator =
                   RuntimeAllocatorConfiguration::make_allocator<ReplyContext>)
      : _node_id(other._node_id, allocator),
        _actor_type(other._actor_type, allocator),
        _instance_id(other._instance_id, allocator) {}

  ReplyContext& operator=(const ReplyContext& other) {
    _node_id = AString(other._node_id, _node_id.get_allocator());
    _actor_type = AString(other._actor_type, _actor_type.get_allocator());
    _instance_id = AString(other._instance_id, _instance_id.get_allocator());
  }

  template <typename PAllocator = allocator_type>
  ReplyContext(ReplyContext&& other,
               const PAllocator& allocator =
                   RuntimeAllocatorConfiguration::make_allocator<ReplyContext>)
      : _node_id(std::move(other._node_id), allocator),
        _actor_type(std::move(other._actor_type), allocator),
        _instance_id(std::move(other._instance_id), allocator) {}

  ReplyContext& operator=(ReplyContext&& other) {
    _node_id = AString(std::move(other._node_id), _node_id.get_allocator());
    _actor_type =
        AString(std::move(other._actor_type), _actor_type.get_allocator());
    _instance_id =
        AString(std::move(other._instance_id), _instance_id.get_allocator());
  }

  void reset() {
    _node_id.clear();
    _actor_type.clear();
    _instance_id.clear();
  }

  void set(const PubSub::Publication& pub) {
    _node_id = AString(*pub.get_str_attr("publisher_node_id"),
                       _node_id.get_allocator());
    _actor_type = AString(*pub.get_str_attr("publisher_actor_type"),
                          _actor_type.get_allocator());
    _instance_id = AString(*pub.get_str_attr("publisher_instance_id"),
                           _instance_id.get_allocator());
  }

  void add_reply_fields(PubSub::Publication* pub) {
    pub->set_attr("node_id", _node_id);
    pub->set_attr("instance_id", _instance_id);
    pub->set_attr("actor_type", _actor_type);
  }

 private:
  AString _node_id;
  AString _actor_type;
  AString _instance_id;
};

}  // namespace uActor::ActorRuntime
