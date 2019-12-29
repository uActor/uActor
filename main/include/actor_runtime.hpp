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
#include "message.hpp"
#include "subscription.hpp"

struct Params {
  const char* node_id;
  const char* instance_id;
};

template <typename ActorType, typename RuntimeType>
class ActorRuntime {
 public:
  static void os_task(void* params) {
    PubSub::Router* router = &PubSub::Router::get_instance();
    const char* instance_id = ((struct Params*)params)->instance_id;
    const char* node_id = ((struct Params*)params)->node_id;

    RuntimeType* runtime = new RuntimeType(router, node_id, instance_id);
    runtime->event_loop();
    delete runtime;

    BoardFunctions::exit_thread();
  }

  explicit ActorRuntime(PubSub::Router* router, const char* node_id,
                        const char* actor_type, const char* instance_id)
      : router_handle(router->new_subscriber()) {
    PubSub::Filter primary_filter{
        PubSub::Constraint(std::string("node_id"), node_id),
        PubSub::Constraint(std::string("actor_type"), actor_type),
        PubSub::Constraint(std::string("instance_id"), instance_id)};
    runtime_subscription_id = router_handle.subscribe(primary_filter);
  }

 protected:
  template <typename... Args>
  void add_actor_base(const Publication& publication, Args... args) {
    const char* node_id = publication.get_attr("spawn_node_id")->data();
    const char* actor_type = publication.get_attr("spawn_actor_type")->data();
    const char* instance_id = publication.get_attr("spawn_instance_id")->data();
    const char* code = publication.get_attr("spawn_code")->data();

    size_t local_id = next_id++;

    actors.try_emplace(local_id, local_id, node_id, actor_type, instance_id,
                       code, std::forward<Args>(args)...);
    size_t sub_id = router_handle.subscribe(PubSub::Filter{
        PubSub::Constraint(std::string("node_id"), node_id),
        PubSub::Constraint(std::string("actor_type"), actor_type),
        PubSub::Constraint(std::string("?instance_id"), instance_id)});
    auto entry = subscription_mapping.find(sub_id);
    if (entry != subscription_mapping.end()) {
      entry->second.emplace_back(local_id);
    } else {
      subscription_mapping.emplace(sub_id, std::list<size_t>{local_id});
    }
  }

 protected:
  std::map<size_t, ActorType> actors;

 private:
  PubSub::SubscriptionHandle router_handle;
  size_t next_id = 1;
  std::map<size_t, std::list<size_t>> subscription_mapping;
  std::list<size_t> ready_queue;
  std::list<std::pair<uint32_t, size_t>> timeouts;
  size_t runtime_subscription_id;

  void event_loop() {
    while (true) {
      // Enqueue from threads master queue
      int32_t wait_time = BoardFunctions::SLEEP_FOREVER;
      if (timeouts.size() > 0) {
        wait_time =
            std::max(0, static_cast<int32_t>(timeouts.begin()->first -
                                             BoardFunctions::timestamp()));
      }
      if (ready_queue.size() > 0) {
        wait_time = 0;
      }
      auto publication = router_handle.receive(wait_time);
      if (publication) {
        if (publication->subscription_id == runtime_subscription_id) {
          if (publication->publication.has_attr("spawn_actor_type")) {
            add_actor_wrapper(publication->publication);
          } else {
            return;
          }
        } else {
          auto receivers =
              subscription_mapping.find(publication->subscription_id);
          if (receivers != subscription_mapping.end()) {
            for (size_t receiver_id : receivers->second) {
              auto actor = actors.find(receiver_id);
              if (actor != actors.end()) {
                if (actor->second.enqueue(
                        Publication(publication->publication))) {
                  if (std::find(ready_queue.begin(), ready_queue.end(),
                                receiver_id) == ready_queue.end()) {
                    ready_queue.push_back(receiver_id);
                  }
                  auto it = std::find_if(timeouts.begin(), timeouts.end(),
                                         [&](auto& element) {
                                           return element.second == receiver_id;
                                         });
                  if (it != timeouts.end()) {
                    timeouts.erase(it);
                  }
                }
              } else {
                printf("outdated reference\n");
              }
            }
          } else {
            printf("not_found\n");
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
          timeouts.pop_front();
        }
      }

      // Process one message
      if (ready_queue.size() > 0) {
        size_t task = ready_queue.front();
        ready_queue.pop_front();
        ActorType& actor = actors.at(task);
        uint32_t wait_time = actor.receive_next_internal();
        if (wait_time == 0) {
          ready_queue.push_back(task);
        } else if (wait_time < UINT32_MAX) {
          timeouts.push_back(
              std::make_pair(BoardFunctions::timestamp() + wait_time, task));
        }
      }
    }
  }

  void add_actor_wrapper(const Publication& publication) {
    static_cast<RuntimeType*>(this)->add_actor(Publication(publication));
  }

  void add_actor(const Publication& publication) {
    add_actor_base<>(publication);
  }
};

#endif  //  MAIN_INCLUDE_ACTOR_RUNTIME_HPP_
