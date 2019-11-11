#ifndef MAIN_MANAGED_ACTOR_HPP_
#define MAIN_MANAGED_ACTOR_HPP_

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <queue>
#include <utility>

#include "message.hpp"
#include "router.hpp"

struct Pattern {
  uint64_t sender;
  uint64_t receiver;

  bool matches(const Message& message) { return true; }
};

class ManagedActor {
 public:
  ManagedActor(const ManagedActor&) = delete;

  ManagedActor(uint64_t id, char* code)
      : waiting(false), _timeout(UINT32_MAX), _id(id) {
    snprintf(_name, sizeof(_name), "actor_%lld", id);
    _code = new char[strlen(code) + 1];
    strncpy(_code, code, strlen(code) + 1);
  }

  virtual void receive(const Message& m) = 0;

  uint32_t receive_next_internal();

  bool enqueue(Message&& message);

  void trigger_timeout() { message_queue.emplace_front(0); }

  uint64_t id() { return _id; }

  char* name() { return _name; }

  char* code() { return _code; }

 protected:
  void send(uint64_t receiver, Message&& m) {
    Router::getInstance().send(_id, receiver, std::move(m));
  }

  void deferred_sleep(uint32_t timeout) {
    _timeout = timeout;
    pattern.receiver = UINT32_MAX;
    pattern.sender = UINT32_MAX;
  }

  void deffered_block_for(uint64_t sender, uint64_t receiver,
                          uint32_t timeout) {
    _timeout = timeout;
    pattern.receiver = receiver;
    pattern.sender = sender;
  }

 private:
  bool waiting;
  Pattern pattern;
  uint32_t _timeout;
  uint64_t _id;
  char _name[32];
  std::deque<Message> message_queue;
  char* _code;
};
#endif  //  MAIN_MANAGED_ACTOR_HPP_
