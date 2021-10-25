#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>

#include "board_functions.hpp"
#include "executor_api.hpp"
#include "managed_actor.hpp"
#include "pubsub/router.hpp"
#include "runtime_allocator_configuration.hpp"
#include "support/logger.hpp"

namespace uActor::ActorRuntime {

struct ExecutorSettings {
  const char* node_id;
  const char* instance_id;
};

template <typename ActorType, typename ExecutorType>
class Executor : public ExecutorApi {
 public:
  template <typename U>
  using Allocator = RuntimeAllocatorConfiguration::Allocator<U>;

  // TODO(raphaelhetzel) this alias seems to cause issues (clang11 macOS
  // segfault) template <typename U> constexpr static auto make_allocator =
  //     RuntimeAllocatorConfiguration::make_allocator<U>;

  using AString =
      std::basic_string<char, std::char_traits<char>, Allocator<char>>;

  static void os_task(void* settings) {
    PubSub::Router* router = &PubSub::Router::get_instance();
    const char* instance_id =
        reinterpret_cast<ExecutorSettings*>(settings)->instance_id;  // NOLINT
    const char* node_id =
        reinterpret_cast<ExecutorSettings*>(settings)->node_id;  // NOLINT

    ExecutorType executor{router, node_id, instance_id};
    executor.event_loop();

    BoardFunctions::exit_thread();
  }

  Executor(PubSub::Router* router, const char* node_id, const char* actor_type,
           const char* instance_id)
      : router_handle(router->new_subscriber()),
        actors(RuntimeAllocatorConfiguration::make_allocator<
               typename decltype(actors)::value_type>()),
        _node_id(node_id),
        _actor_type(actor_type),
        _instance_id(instance_id),
        subscription_mapping(
            RuntimeAllocatorConfiguration::make_allocator<
                typename decltype(subscription_mapping)::value_type>()),
        ready_queue(RuntimeAllocatorConfiguration::make_allocator<
                    typename decltype(ready_queue)::value_type>()),
        timeouts(RuntimeAllocatorConfiguration::make_allocator<
                 typename decltype(timeouts)::value_type>()),
        delayed_messages(RuntimeAllocatorConfiguration::make_allocator<
                         typename decltype(delayed_messages)::value_type>()) {
    PubSub::Filter primary_filter{
        PubSub::Constraint(std::string("node_id"), node_id),
        PubSub::Constraint(std::string("actor_type"), actor_type),
        PubSub::Constraint(std::string("instance_id"), instance_id)};
    executor_subscription_id = router_handle.subscribe(
        primary_filter,
        PubSub::ActorIdentifier(node_id, actor_type, instance_id));
  }

  Executor(const Executor&) = delete;
  Executor(Executor&&) = delete;
  Executor& operator=(const Executor&) = delete;
  Executor& operator=(Executor&&) = delete;

 protected:
  template <typename... Args>
  void add_actor_base(const PubSub::Publication& publication, Args... args) {
    assert(publication.has_attr("spawn_actor_version"));
    auto node_id =
        std::get<std::string_view>(publication.get_attr("spawn_node_id"));
    auto actor_type =
        std::get<std::string_view>(publication.get_attr("spawn_actor_type"));
    auto actor_version =
        std::get<std::string_view>(publication.get_attr("spawn_actor_version"));
    auto instance_id =
        std::get<std::string_view>(publication.get_attr("spawn_instance_id"));

    uint32_t local_id = next_id++;

    if (auto [actor_it, success] = actors.try_emplace(
            local_id, this, local_id, node_id, actor_type, actor_version,
            instance_id, std::forward<Args>(args)...);
        success) {
      // TODO(raphaelhetzel) clean up
      actor_it->second.add_default_subscription();
      actor_it->second.publish_creation_message();
      auto result = actor_it->second.early_initialize();
      if (result.second < UINT32_MAX) {
        timeouts.emplace(BoardFunctions::timestamp() + result.second,
                         actor_it->first, false, AString());
      }
      auto init_message =
          PubSub::Publication(_node_id, _actor_type, _instance_id);
      init_message.set_attr("node_id", node_id);
      init_message.set_attr("actor_type", actor_type);
      init_message.set_attr("instance_id", instance_id);
      init_message.set_attr("type", "init");
      PubSub::Router::get_instance().publish(std::move(init_message));
    }
  }

  uint32_t add_subscription(uint32_t local_id, PubSub::Filter&& filter,
                            uint8_t priority) override {
    auto actor_it = actors.find(local_id);
    if (actor_it == actors.end()) {
      return 0;
    }
    uint32_t sub_id = router_handle.subscribe(
        filter,
        PubSub::ActorIdentifier(actor_it->second.node_id(),
                                actor_it->second.actor_type(),
                                actor_it->second.instance_id()),
        priority);
    auto entry = subscription_mapping.find(sub_id);
    if (entry != subscription_mapping.end()) {
      entry->second.insert(local_id);
    } else {
      subscription_mapping.emplace(sub_id, std::set<uint32_t>{local_id});
    }
    return sub_id;
  }

  void remove_subscription(uint32_t local_id, uint32_t sub_id) override {
    auto it = subscription_mapping.find(sub_id);
    if (it != subscription_mapping.end()) {
      it->second.erase(local_id);
      if (it->second.empty()) {
        router_handle.unsubscribe(sub_id);
        subscription_mapping.erase(it);
      }
    }
  }

  void delayed_publish(PubSub::Publication&& publication,
                       uint32_t delay) override {
    delayed_messages.emplace(BoardFunctions::timestamp() + delay,
                             std::move(publication));
  }

  void enqueue_wakeup(uint32_t actor_id, uint32_t delay,
                      std::string_view wakeup_id) override {
    uint32_t new_timeout = BoardFunctions::timestamp() + delay;
    timeouts.emplace(new_timeout, actor_id, true, AString(wakeup_id));
  }

 private:
  PubSub::ReceiverHandle router_handle;

 protected:
  std::map<uint32_t, ActorType, std::less<uint32_t>,
           std::scoped_allocator_adaptor<
               Allocator<std::pair<const uint32_t, ActorType>>>>
      actors;

  std::string_view node_id() { return std::string_view(_node_id); }

  std::string_view actor_type() { return std::string_view(_actor_type); }

  std::string_view instance_id() { return std::string_view(_instance_id); }

 private:
  AString _node_id;
  AString _actor_type;
  AString _instance_id;
  uint32_t next_id = 1;
  std::map<uint32_t, std::set<uint32_t>, std::less<uint32_t>,
           std::scoped_allocator_adaptor<
               Allocator<std::pair<const uint32_t, std::set<uint32_t>>>>>
      subscription_mapping;
  std::list<uint32_t, Allocator<uint32_t>> ready_queue;

  struct Timeout {
    Timeout(uint32_t timeout, uint32_t actor_id, bool user_defined,
            AString&& trigger_id)
        : timeout(timeout),
          actor_id(actor_id),
          user_defined(user_defined),
          trigger_id(std::move(trigger_id)) {}

    uint32_t timeout;
    uint32_t actor_id;
    bool user_defined;
    AString trigger_id;

    bool operator<(const Timeout& other) const {
      if (other.timeout != timeout) {
        return timeout < other.timeout;
      } else if (other.actor_id != actor_id) {
        return actor_id < other.actor_id;
      } else if (other.user_defined != user_defined) {
        return user_defined < other.user_defined;
      } else {
        return trigger_id < other.trigger_id;
      }
    }

    bool operator==(const Timeout& other) const {
      return other.timeout == timeout && other.actor_id == actor_id &&
             other.user_defined == user_defined &&
             other.trigger_id == trigger_id;
    }
  };

  constexpr static auto timeout_cmp = [](const auto& left, const auto& right) {
    return left.timeout > right.timeout;
  };
  std::set<Timeout, std::less<Timeout>, Allocator<Timeout>> timeouts;

  std::multimap<uint32_t, PubSub::Publication, std::less<uint32_t>,
                Allocator<std::pair<const uint32_t, PubSub::Publication>>>
      delayed_messages;
  uint32_t executor_subscription_id;

  void event_loop() {
    while (true) {
      // Enqueue from threads master queue

      uint32_t wait_time = BoardFunctions::SLEEP_FOREVER;
      if (!timeouts.empty()) {
        wait_time =
            std::max(0, static_cast<int32_t>(timeouts.begin()->timeout -
                                             BoardFunctions::timestamp()));
      }
      if (!delayed_messages.empty()) {
        wait_time = std::min(
            wait_time,
            static_cast<uint32_t>(std::max(
                0, static_cast<int32_t>(delayed_messages.begin()->first -
                                        BoardFunctions::timestamp()))));
      }
      if (!ready_queue.empty()) {
        wait_time = 0;
      }
      auto publication = router_handle.receive(wait_time);
      while (publication) {
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
            int receiver_count = receivers->second.size();
            for (uint32_t receiver_id : receivers->second) {
              auto actor = actors.find(receiver_id);
              if (actor != actors.end()) {
                auto to_send =
                    (receiver_count == 1)
                        ? std::move(publication->publication)
                        : PubSub::Publication(publication->publication);
                receiver_count--;
                if (actor->second.enqueue(std::move(to_send))) {
                  // TODO(raphaelhetzel): Remove the linear finds
                  if (std::find(ready_queue.begin(), ready_queue.end(),
                                receiver_id) == ready_queue.end()) {
                    ready_queue.push_back(receiver_id);
                  }
                  auto it =
                      std::find_if(timeouts.begin(), timeouts.end(),
                                   [receiver_id](const auto& timeout) {
                                     return timeout.actor_id == receiver_id &&
                                            timeout.user_defined == false;
                                   });
                  if (it != timeouts.end()) {
                    timeouts.erase(it);
                  }
                }
              } else {
                Support::Logger::warning("EXECUTOR", "Outdated reference");
              }
            }
          } else {
            // Message got published before the actor was deleted.
            // NO-OP
          }
        }
        publication = router_handle.receive(0);
      }

      while (delayed_messages.begin() != delayed_messages.end() &&
             delayed_messages.begin()->first < BoardFunctions::timestamp()) {
        PubSub::Router::get_instance().publish(
            std::move(delayed_messages.begin()->second));
        delayed_messages.erase(delayed_messages.begin());
      }

      // Enqueue from timeouts
      auto start = BoardFunctions::timestamp();
      while (!timeouts.empty() && timeouts.begin()->timeout < start) {
        const auto& timeout = *timeouts.begin();
        if (timeout.timeout < start) {
          auto actor_it = actors.find(timeout.actor_id);
          if (actor_it != actors.end()) {
            actor_it->second.trigger_timeout(timeout.user_defined,
                                             timeout.trigger_id);
            if (std::find(ready_queue.begin(), ready_queue.end(),
                          timeout.actor_id) == ready_queue.end()) {
              ready_queue.push_back(timeout.actor_id);
            }
          }
          timeouts.erase(timeouts.begin());
        }
      }

      // Process one message
      if (!ready_queue.empty()) {
        uint32_t task = ready_queue.front();
        ready_queue.pop_front();
        auto actor_it = actors.find(task);
        if (actor_it != actors.end()) {
          // testbed_start_timekeeping("execution");
          ManagedActor::ReceiveResult result =
              actor_it->second.receive_next_internal();
          // testbed_stop_timekeeping("execution");
          if (result.exit) {
            actors.erase(actor_it);
          } else if (result.next_timeout == 0) {
            ready_queue.push_back(task);
          } else if (result.next_timeout < UINT32_MAX) {
            timeouts.emplace(BoardFunctions::timestamp() + result.next_timeout,
                             task, false, AString());
          }
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
