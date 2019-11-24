#ifndef MAIN_INCLUDE_MANAGED_ACTOR_HPP_
#define MAIN_INCLUDE_MANAGED_ACTOR_HPP_

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <queue>
#include <utility>

#include "message.hpp"
#include "router_v2.hpp"

struct Pattern {
  char sender[128];
  char receiver[128];
  uint32_t tag;

  bool matches(const Message& message) {
    return strcmp(message.sender(), sender) == 0 &&
           strcmp(message.receiver(), receiver) == 0 && message.tag() == tag;
  }
};

class ManagedActor {
 public:
  ManagedActor(const ManagedActor&) = delete;

  ManagedActor(const char* id, const char* code)
      : waiting(false), _timeout(UINT32_MAX) {
    strncpy(_id, id, sizeof(_id));
    _code = new char[strlen(code) + 1];
    strncpy(_code, code, strlen(code) + 1);
  }

  virtual void receive(const Message& m) = 0;

  uint32_t receive_next_internal();

  bool enqueue(Message&& message);

  void trigger_timeout() {
    message_queue.emplace_front(id(), id(), Tags::WELL_KNOWN_TAGS::TIMEOUT,
                                "timeout");
  }

  char* id() { return _id; }

  char* code() { return _code; }

 protected:
  void send(Message&& m) { RouterV2::getInstance().send(std::move(m)); }

  void deferred_sleep(uint32_t timeout) {
    waiting = true;
    strncpy(pattern.receiver, _id, sizeof(pattern.receiver));
    strncpy(pattern.sender, _id, sizeof(pattern.sender));
    pattern.tag = Tags::WELL_KNOWN_TAGS::TIMEOUT;
    _timeout = timeout;
  }

  void deffered_block_for(const char* sender, const char* receiver,
                          uint32_t tag, uint32_t timeout) {
    waiting = true;
    strncpy(pattern.sender, sender, sizeof(pattern.sender));
    strncpy(pattern.receiver, receiver, sizeof(pattern.receiver));
    pattern.tag = tag;
    _timeout = timeout;
  }

 private:
  bool waiting;
  Pattern pattern;
  uint32_t _timeout;
  char _id[128];
  std::deque<Message> message_queue;
  char* _code;
};
#endif  //  MAIN_INCLUDE_MANAGED_ACTOR_HPP_
