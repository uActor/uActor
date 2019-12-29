#ifndef MAIN_INCLUDE_MANAGED_ACTOR_HPP_
#define MAIN_INCLUDE_MANAGED_ACTOR_HPP_

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "message.hpp"
#include "publication.hpp"
#include "subscription.hpp"

class Pattern {
 public:
  Pattern() : filter() {}

  bool matches(const Publication& publication) {
    return filter.matches(publication);
  }

  PubSub::Filter filter;
};

class ManagedActor {
 public:
  ManagedActor(const ManagedActor&) = delete;

  ManagedActor(size_t local_id, const char* node_id, const char* actor_type,
               const char* instance_id, const char* code)
      : _id(local_id),
        waiting(false),
        _timeout(UINT32_MAX),
        _node_id(node_id),
        _actor_type(actor_type),
        _instance_id(instance_id),
        _code(code) {}

  virtual void receive(const Publication& p) = 0;

  uint32_t receive_next_internal();

  bool enqueue(Publication&& message);

  void trigger_timeout() {
    Publication p = Publication(_node_id.c_str(), _actor_type.c_str(),
                                _instance_id.c_str());
    p.set_attr("_internal_timeout", "_timeout");
    message_queue.emplace_front(std::move(p));
  }

  const char* code() { return _code.c_str(); }

  size_t id() { return _id; }

  const char* node_id() const { return _node_id.c_str(); }

  const char* actor_type() const { return _actor_type.c_str(); }

  const char* instance_id() const { return _instance_id.c_str(); }

 protected:
  void send(Publication&& p) {
    PubSub::Router::get_instance().publish(std::move(p));
  }

  void deferred_sleep(uint32_t timeout) {
    waiting = true;
    pattern.filter.clear();
    pattern.filter = PubSub::Filter{
        PubSub::Constraint{"internal_timeout", "internal_timeout"}};
    _timeout = timeout;
  }

  void deffered_block_for(PubSub::Filter&& filter, uint32_t timeout) {
    waiting = true;
    pattern.filter = std::move(filter);
    _timeout = timeout;
  }

 private:
  size_t _id;
  bool waiting;
  Pattern pattern;
  uint32_t _timeout;
  std::string _node_id;
  std::string _actor_type;
  std::string _instance_id;
  std::string _code;
  std::deque<Publication> message_queue;
};
#endif  //  MAIN_INCLUDE_MANAGED_ACTOR_HPP_
