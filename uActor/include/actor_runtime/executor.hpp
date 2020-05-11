#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_EXECUTOR_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_EXECUTOR_HPP_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "board_functions.hpp"
#include "executor_api.hpp"
#include "managed_actor.hpp"
#include "pubsub/router.hpp"

namespace uActor::ActorRuntime {

struct ExecutorSettings {
  const char* node_id;
  const char* instance_id;
};

template <typename ActorType, typename ExecutorType>
class Executor : public ExecutorApi {
 public:
  static void os_task(void* settings) {
    auto* router = &PubSub::Router::get_instance();
    const char* instance_id = ((struct ExecutorSettings*)settings)->instance_id;
    const char* node_id = ((struct ExecutorSettings*)settings)->node_id;

    ExecutorType* executor = new ExecutorType(router, node_id, instance_id);
    executor->event_loop();
    delete executor;

    BoardFunctions::exit_thread();
  }

  Executor(PubSub::Router* router, const char* node_id, const char* actor_type,
           const char* instance_id)
      : _node_id(node_id),
        _actor_type(actor_type),
        _instance_id(instance_id),
        router_handle(router->new_subscriber()) {
    PubSub::Filter primary_filter{
        PubSub::Constraint(std::string("node_id"), node_id),
        PubSub::Constraint(std::string("actor_type"), actor_type),
        PubSub::Constraint(std::string("instance_id"), instance_id)};
    executor_subscription_id = router_handle.subscribe(primary_filter);
  }

 protected:
  template <typename... Args>
  void add_actor_base(const PubSub::Publication& publication, Args... args) {
    const char* node_id =
        std::get<std::string_view>(publication.get_attr("spawn_node_id"))
            .data();
    const char* actor_type =
        std::get<std::string_view>(publication.get_attr("spawn_actor_type"))
            .data();
    const char* instance_id =
        std::get<std::string_view>(publication.get_attr("spawn_instance_id"))
            .data();
    const char* code =
        std::get<std::string_view>(publication.get_attr("spawn_code")).data();

    uint32_t local_id = next_id++;

    if (auto [actor_it, success] =
            actors.try_emplace(local_id, this, local_id, node_id, actor_type,
                               instance_id, code, std::forward<Args>(args)...);
        success) {
      if (actor_it->second.initialize()) {
        auto init_message =
            PubSub::Publication(_node_id, _actor_type, _instance_id);
        init_message.set_attr("node_id", node_id);
        init_message.set_attr("actor_type", actor_type);
        init_message.set_attr("instance_id", instance_id);
        init_message.set_attr("type", "init");
        PubSub::Router::get_instance().publish(std::move(init_message));
      } else {
        actors.erase(actor_it);
      }
    }
  }

  uint32_t add_subscription(uint32_t local_id, PubSub::Filter&& filter) {
    uint32_t sub_id = router_handle.subscribe(filter);
    auto entry = subscription_mapping.find(sub_id);
    if (entry != subscription_mapping.end()) {
      entry->second.insert(local_id);
    } else {
      subscription_mapping.emplace(sub_id, std::set<uint32_t>{local_id});
    }
    return sub_id;
  }

  void remove_subscription(uint32_t local_id, uint32_t sub_id) {
    auto it = subscription_mapping.find(sub_id);
    if (it != subscription_mapping.end()) {
      it->second.erase(local_id);
      if (it->second.empty()) {
        router_handle.unsubscribe(sub_id);
        subscription_mapping.erase(it);
      }
    }
  }

  void delayed_publish(PubSub::Publication&& publication, uint32_t delay) {
    delayed_messages.emplace(BoardFunctions::timestamp() + delay,
                             std::move(publication));
  }

 protected:
  std::map<uint32_t, ActorType> actors;

 private:
  std::string _node_id;
  std::string _actor_type;
  std::string _instance_id;
  PubSub::ReceiverHandle router_handle;
  uint32_t next_id = 1;
  std::map<uint32_t, std::set<uint32_t>> subscription_mapping;
  std::list<uint32_t> ready_queue;
  std::list<std::pair<uint32_t, uint32_t>> timeouts;
  std::multimap<uint32_t, PubSub::Publication> delayed_messages;
  uint32_t executor_subscription_id;

  void event_loop() {
    while (true) {
      // Enqueue from threads master queue
      uint32_t wait_time = BoardFunctions::SLEEP_FOREVER;
      if (timeouts.size() > 0) {
        wait_time =
            std::max(0, static_cast<int32_t>(timeouts.begin()->first -
                                             BoardFunctions::timestamp()));
      }
      if (delayed_messages.size() > 0) {
        wait_time = std::min(
            wait_time,
            static_cast<uint32_t>(std::max(
                0, static_cast<int32_t>(delayed_messages.begin()->first -
                                        BoardFunctions::timestamp()))));
      }
      if (ready_queue.size() > 0) {
        wait_time = 0;
      }
      auto publication = router_handle.receive(wait_time);
      if (publication) {
        if (publication->subscription_id == executor_subscription_id) {
          if (publication->publication.has_attr("spawn_actor_type")) {
            add_actor_wrapper(publication->publication);
          } else {
            return;
          }
        } else {
          auto receivers =
              subscription_mapping.find(publication->subscription_id);
          if (receivers != subscription_mapping.end()) {
            for (uint32_t receiver_id : receivers->second) {
              auto actor = actors.find(receiver_id);
              if (actor != actors.end()) {
                if (actor->second.enqueue(PubSub::Publication(
                        std::move(publication->publication)))) {
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
            // Message got published before the actor was deleted.
            // NO-OP
          }
        }
      }

      while (delayed_messages.begin() != delayed_messages.end() &&
             delayed_messages.begin()->first < BoardFunctions::timestamp()) {
        PubSub::Router::get_instance().publish(
            std::move(delayed_messages.begin()->second));
        delayed_messages.erase(delayed_messages.begin());
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
        uint32_t task = ready_queue.front();
        ready_queue.pop_front();
        ActorType& actor = actors.at(task);
        // testbed_start_timekeeping("execution");
        ManagedActor::ReceiveResult result = actor.receive_next_internal();
        // testbed_stop_timekeeping("execution");
        if (result.exit) {
          actors.erase(task);
        } else if (result.next_timeout == 0) {
          ready_queue.push_back(task);
        } else if (result.next_timeout < UINT32_MAX) {
          timeouts.push_back(std::make_pair(
              BoardFunctions::timestamp() + result.next_timeout, task));
        }
      }
    }
  }

  void add_actor_wrapper(const PubSub::Publication& publication) {
    static_cast<ExecutorType*>(this)->add_actor(
        PubSub::Publication(publication));
  }

  void add_actor(const PubSub::Publication& publication) {
    add_actor_base<>(publication);
  }
};

}  //  namespace uActor::ActorRuntime

#endif  //  UACTOR_INCLUDE_ACTOR_RUNTIME_EXECUTOR_HPP_
