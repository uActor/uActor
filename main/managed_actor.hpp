#ifndef MAIN_MANAGED_ACTOR_HPP_
#define MAIN_MANAGED_ACTOR_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <unordered_map>
#include <utility>

#include "message.hpp"
#include "router.hpp"

struct Params {
  uint64_t id;
  Router* router;
};

struct Pattern {
  uint64_t sender;
  uint64_t receiver;

  bool matches(const Message& message) { return true; }
};

class ManagedActor {
 public:
  ManagedActor(uint64_t id, char* code)
      : waiting(false), _timeout(UINT32_MAX), _id(id) {
    snprintf(_name, sizeof(_name), "actor_%lld", id);
  }

  virtual void receive(const Message& m) = 0;

  uint32_t receive_next_internal();

  bool enqueue(Message&& message) {
    if (waiting && pattern.matches(message)) {
      message_queue.emplace_front(std::move(message));
      return false;
    } else {
      message_queue.emplace_back(std::move(message));
      return true;
    }
  }

  void trigger_timeout() { message_queue.emplace_front(new Message(true)); }

  uint64_t id() { return _id; }

  char* name() { return _name; }

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
};

template <typename ActorType>
class ActorRuntime {
 public:
  explicit ActorRuntime(Router* router, uint64_t id) : id(id), router(router) {
    router->register_actor(id);
  }
  void event_loop() {
    while (true) {
      // Enqueue from threads master queue
      auto message = router->receive(id);
      if (message) {
        if (message->receiver() == id) {
          add_actor(message->buffer());
        } else {
          auto receiver = actors.find(message->receiver());
          if (receiver != actors.end()) {
            if (receiver->second.enqueue(std::move(*message))) {
              if (std::find(ready_queue.begin(), ready_queue.end(),
                            receiver->second.id()) == ready_queue.end()) {
                ready_queue.push_back(receiver->second.id());
              }
            }
          }
        }
      }

      // Enqueue from timeouts
      if (timeouts.begin() != timeouts.end()) {
        if (timeouts.begin()->first < xTaskGetTickCount()) {
          if (std::find(ready_queue.begin(), ready_queue.end(),
                        timeouts.begin()->second) == ready_queue.end()) {
            actors.at(timeouts.begin()->second).trigger_timeout();
            ready_queue.push_back(timeouts.begin()->second);
          }
          timeouts.erase(timeouts.begin());
        }
      }

      // Process one message
      if (ready_queue.size() > 0) {
        uint64_t task = ready_queue.front();
        ready_queue.pop_front();
        ActorType& actor = actors.at(task);
        uint32_t wait_time = actor.receive_next_internal();
        if (wait_time == 0) {
          ready_queue.push_back(actor.id());
        } else if (wait_time < UINT32_MAX) {
          timeouts.insert(std::make_pair(
              xTaskGetTickCount() + wait_time / portTICK_PERIOD_MS,
              actor.id()));
        }
      }
    }
  }

 private:
  uint64_t id;
  Router* router;
  std::list<uint64_t> ready_queue;
  std::map<uint32_t, uint64_t> timeouts;
  std::unordered_map<uint64_t, ActorType> actors;

  void add_actor(char* payload) {
    uint64_t actor_id = *reinterpret_cast<uint64_t*>(payload);
    char* code = payload + 8;
    actors.emplace(actor_id, create_actor(actor_id, code));
    router->register_alias(id, actor_id);
  }

  virtual ActorType create_actor(uint64_t actor_id, char* code) = 0;
};
#endif  //  MAIN_MANAGED_ACTOR_HPP_
