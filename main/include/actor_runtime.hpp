#ifndef MAIN_INCLUDE_ACTOR_RUNTIME_HPP_
#define MAIN_INCLUDE_ACTOR_RUNTIME_HPP_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "board_functions.hpp"
#include "router_v2.hpp"

struct Params {
  char* id;
  RouterV2* router;
};

template <typename ActorType, typename RuntimeType>
class ActorRuntime {
 public:
  static void os_task(void* params) {
    RouterV2* router = ((struct Params*)params)->router;
    char* id = ((struct Params*)params)->id;
    RuntimeType* runtime = new RuntimeType(router, id);
    runtime->event_loop();
    BoardFunctions::exit_thread();
  }

  explicit ActorRuntime(RouterV2* router, char* id) : id(id), router(router) {
    router->register_actor(id);
  }

 protected:
  template <typename... Args>
  void add_actor_base(const char* payload, Args... args) {
    const char* actor_id = payload;
    const char* code = payload + strlen(payload) + 1;
    actors.try_emplace(std::hash<std::string>{}(std::string(actor_id)),
                       actor_id, code, std::forward<Args>(args)...);
    router->register_alias(id, actor_id);
  }

 private:
  char* id;
  RouterV2* router;
  std::list<size_t> ready_queue;
  std::map<uint32_t, size_t> timeouts;
  std::unordered_map<size_t, ActorType> actors;

  void event_loop() {
    while (true) {
      // Enqueue from threads master queue
      size_t wait_time = BoardFunctions::SLEEP_FOREVER;
      if (timeouts.size() > 0) {
        wait_time = (timeouts.begin()->first - BoardFunctions::timestamp());
      }
      if (ready_queue.size() > 0) {
        wait_time = 0;
      }
      auto message = router->receive(id, wait_time);
      if (message) {
        if (!strcmp(message->receiver(), id)) {
          if (message->tag() == Tags::WELL_KNOWN_TAGS::EXIT) {
            return;
          } else {
            add_actor_wrapper(message->buffer());
          }
        } else {
          auto receiver = actors.find(
              std::hash<std::string>{}(std::string(message->receiver())));
          if (receiver != actors.end()) {
            size_t hash = std::hash<std::string>{}(receiver->second.id());
            if (receiver->second.enqueue(std::move(*message))) {
              if (std::find(ready_queue.begin(), ready_queue.end(), hash) ==
                  ready_queue.end()) {
                ready_queue.push_back(hash);
              }
              auto it = std::find_if(
                  timeouts.begin(), timeouts.end(),
                  [&](auto& element) { return element.second == hash; });
              if (it != timeouts.end()) {
                timeouts.erase(it);
              }
            }
          } else {
            printf("not_found %s\n", message->receiver());
          }
        }
      }

      // Enqueue from timeouts
      if (timeouts.begin() != timeouts.end()) {
        if (timeouts.begin()->first < BoardFunctions::timestamp()) {
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
        size_t task = ready_queue.front();
        ready_queue.pop_front();
        ActorType& actor = actors.at(task);
        uint32_t wait_time = actor.receive_next_internal();
        if (wait_time == 0) {
          ready_queue.push_back(
              std::hash<std::string>{}(std::string(actor.id())));
        } else if (wait_time < UINT32_MAX) {
          timeouts.emplace(BoardFunctions::timestamp() + wait_time,
                           std::hash<std::string>{}(std::string(actor.id())));
        }
      }
    }
  }

  void add_actor_wrapper(const char* payload) {
    static_cast<RuntimeType*>(this)->add_actor(payload);
  }

  void add_actor(const char* payload) { add_actor_base<>(payload); }
};

#endif  //  MAIN_INCLUDE_ACTOR_RUNTIME_HPP_
