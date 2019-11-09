#ifndef MAIN_ACTOR_RUNTIME_HPP_
#define MAIN_ACTOR_RUNTIME_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>
#include <utility>

#include "router.hpp"

struct Params {
  uint64_t id;
  Router* router;
};

template <typename ActorType, typename RuntimeType>
class ActorRuntime {
 public:
  static void os_task(void* params) {
    Router* router = ((struct Params*)params)->router;
    uint64_t id = ((struct Params*)params)->id;
    RuntimeType* runtime = new RuntimeType(router, id);
    runtime->event_loop();
  }

  explicit ActorRuntime(Router* router, uint64_t id) : id(id), router(router) {
    router->register_actor(id);
  }

 protected:
  template <typename... Args>
  void add_actor_base(char* payload, Args... args) {
    uint64_t actor_id = *reinterpret_cast<uint64_t*>(payload);
    char* code = payload + 8;
    actors.try_emplace(actor_id, actor_id, code, std::forward<Args>(args)...);
    router->register_alias(id, actor_id);
  }

 private:
  uint64_t id;
  Router* router;
  std::list<uint64_t> ready_queue;
  std::map<uint32_t, uint64_t> timeouts;
  std::unordered_map<uint64_t, ActorType> actors;

  void event_loop() {
    while (true) {
      // Enqueue from threads master queue
      auto message = router->receive(id);
      if (message) {
        if (message->receiver() == id) {
          add_actor_wrapper(message->buffer());
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

  void add_actor_wrapper(char* payload) {
    static_cast<RuntimeType*>(this)->add_actor(payload);
  }

  void add_actor(char* payload) { add_actor_base<>(payload); }
};

#endif  //  MAIN_ACTOR_RUNTIME_HPP_
