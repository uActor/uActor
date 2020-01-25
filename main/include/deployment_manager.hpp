#ifndef MAIN_INCLUDE_DEPLOYMENT_MANAGER_HPP_
#define MAIN_INCLUDE_DEPLOYMENT_MANAGER_HPP_

#include <cassert>
#include <cstdint>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "native_actor.hpp"
#include "publication.hpp"
#include "subscription.hpp"

struct Deployment {
  Deployment(std::string_view name, std::string_view actor_type,
             std::string_view actor_version, std::string_view actor_code,
             std::string_view actor_runtime_type,
             std::string_view raw_required_actors, uint32_t lifetime_end)
      : name(name),
        actor_type(actor_type),
        actor_version(actor_version),
        actor_code(actor_code),
        actor_runtime_type(actor_runtime_type),
        lifetime_end(lifetime_end) {
    process_raw_required_actors(raw_required_actors);
  }

  std::string name;

  std::string actor_type;
  std::string actor_version;
  std::string actor_code;
  std::string actor_runtime_type;

  std::list<std::string> required_actors;

  uint32_t lifetime_subscription_id = 0;
  int32_t restarts = -1;
  uint32_t lifetime_end;
  bool active = false;

 private:
  // Adapted string_view substring algorithm from
  // https://www.bfilipek.com/2018/07/string-view-perf-followup.html
  void process_raw_required_actors(std::string_view raw_required_actors) {
    size_t pos = 0;
    while (pos < raw_required_actors.size()) {
      const auto end_pos = raw_required_actors.find_first_of(",", pos);
      if (pos != end_pos) {
        required_actors.emplace_back(
            raw_required_actors.substr(pos, end_pos - pos));
      }
      if (end_pos == std::string_view::npos) {
        break;
      }
      pos = end_pos + 1;
    }
  }
};

struct Runtime {
  Runtime(std::string_view node_id, std::string_view actor_type,
          std::string_view instance_id)
      : node_id(node_id), actor_type(actor_type), instance_id(instance_id) {}
  std::string node_id;
  std::string actor_type;
  std::string instance_id;
};

class DeploymentManager : public NativeActor {
 public:
  DeploymentManager(ManagedNativeActor* actor_wrapper, std::string_view node_id,
                    std::string_view actor_type, std::string_view instance_id)
      : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {
    subscribe(PubSub::Filter{PubSub::Constraint{"type", "deployment"}});
    runtimes.try_emplace("lua");
    runtimes.at("lua").push_back(Runtime(node_id, "lua_runtime", "1"));

    std::string fake_node_actor =
        std::string("fake.node_id.") + std::string(node_id);
    increment_deployed_actor_type_count(fake_node_actor);
  }

  void receive(const Publication& publication) {
    if (publication.get_str_attr("type") == "deployment") {
      receive_deployment(publication);
    } else if (publication.get_str_attr("category") == "actor_lifetime") {
      receive_lifetime_event(publication);
    } else if (publication.get_str_attr("type") == "deployment_lifetime_end") {
      receive_ttl_timeout(publication);
    }
  }

 private:
  std::unordered_map<std::string, std::list<Runtime>> runtimes;

  std::unordered_map<std::string, Deployment> deployments;
  std::unordered_map<std::string, std::unordered_set<Deployment*>> dependencies;
  std::unordered_map<uint32_t, std::string> subscriptions;

  std::map<uint32_t, std::list<std::string>> ttl_end_times;

  std::unordered_map<std::string, uint32_t> deployed_actor_types;

  void receive_deployment(const Publication& publication) {
    if (auto runtime_it = runtimes.find(std::string(
            *publication.get_str_attr("deployment_actor_runtime_type")));
        runtime_it != runtimes.end()) {
      Runtime& runtime = runtime_it->second.front();
      if (publication.has_attr("deployment_name") &&
          publication.has_attr("deployment_ttl") &&
          publication.has_attr("deployment_actor_type") &&
          publication.has_attr("deployment_actor_version") &&
          publication.has_attr("deployment_actor_code") &&
          publication.has_attr("deployment_required_actors")) {
        printf("Deployment Message\n");

        std::string_view name = *publication.get_str_attr("deployment_name");
        std::string_view actor_type =
            *publication.get_str_attr("deployment_actor_type");
        std::string_view actor_version =
            *publication.get_str_attr("deployment_actor_version");
        std::string_view actor_code =
            *publication.get_str_attr("deployment_actor_code");
        std::string_view actor_runtime_type =
            *publication.get_str_attr("deployment_actor_runtime_type");
        std::string_view required_actors =
            *publication.get_str_attr("deployment_required_actors");

        uint32_t end_time = 0;
        if (*publication.get_int_attr("deployment_ttl") > 0) {
          end_time = now() + *publication.get_int_attr("deployment_ttl");
        }

        auto [deployment_iterator, inserted] = deployments.try_emplace(
            std::string(name), name, actor_type, actor_version, actor_code,
            actor_runtime_type, required_actors, end_time);

        Deployment& deployment = deployment_iterator->second;

        if (inserted) {
          printf("Deployment Created\n");

          add_deployment_dependencies(&deployment);
          if (requirements_check(&deployment)) {
            activate_deployment(&deployment, &runtime);
          }
        } else {
          printf("Deployment Update / Refresh\n");

          if (actor_version != deployment.actor_version ||
              actor_type != deployment.actor_type) {
            stop_deployment(&deployment);
            decrement_deployed_actor_type_count(deployment.actor_type);
            deployment.actor_type = actor_type;
            deployment.actor_version = actor_version;
            deployment.actor_code = actor_code;
            increment_deployed_actor_type_count(deployment.actor_type);
            start_deployment(&deployment, &runtime);
          }

          deployment.lifetime_end = end_time;
          enqueue_lifetime_end_wakeup(&deployment);
        }
      }
    }
  }

  void receive_lifetime_event(const Publication& publication) {
    printf("Deployment Manager Lifetime Event %s\n",
           publication.get_str_attr("type")->data());
    if (publication.get_str_attr("type") == "actor_exit") {
      std::string deployment_id =
          std::string(*publication.get_str_attr("lifetime_instance_id"));
      if (const auto deployment_it = deployments.find(deployment_id);
          deployment_it != deployments.end()) {
        Deployment& deployment = deployment_it->second;
        if (deployment.lifetime_end < now()) {
          printf("Deployment lifetime ended\n");
          deactivate_deployment(&deployment);
          remove_deployment(&deployment);
        } else if (*publication.get_str_attr("exit_reason") != "clean_exit") {
          if (deployment.restarts < 3) {
            if (auto runtime_list_it =
                    runtimes.find(deployment.actor_runtime_type);
                runtime_list_it != runtimes.end()) {
              Runtime& runtime = runtime_list_it->second.front();
              start_deployment(&deployment, &runtime);
            }
          } else {
            deactivate_deployment(&deployment);
            remove_deployment(&deployment);
          }
        }
      }
    }
  }

  void receive_ttl_timeout(const Publication& publication) {
    while (ttl_end_times.begin() != ttl_end_times.end() &&
           ttl_end_times.begin()->first < now()) {
      printf("Deployment Manager Timeout\n");
      for (auto deployment_id : ttl_end_times.begin()->second) {
        if (auto deployment_it = deployments.find(deployment_id);
            deployment_it != deployments.end()) {
          Deployment& deployment = deployment_it->second;

          if (ttl_end_times.begin()->first == deployment.lifetime_end) {
            deactivate_deployment(&deployment);
            remove_deployment(&deployment);
          }
        }
      }
      ttl_end_times.erase(ttl_end_times.begin());
    }
  }

  bool requirements_check(Deployment* deployment) {
    for (const auto& actor_type : deployment->required_actors) {
      if (auto it = deployed_actor_types.find(actor_type);
          it == deployed_actor_types.end()) {
        return false;
      }
    }
    return true;
  }

  void activate_deployment(Deployment* deployment, Runtime* runtime) {
    increment_deployed_actor_type_count(deployment->actor_type);
    deployment->active = true;

    if (deployment->lifetime_subscription_id == 0) {
      uint32_t sub_id = subscribe(
          PubSub::Filter{PubSub::Constraint{"category", "actor_lifetime"},
                         PubSub::Constraint{"?lifetime_instance_id",
                                            std::string(deployment->name)}});
      subscriptions.emplace(sub_id, deployment->name);
      deployment->lifetime_subscription_id = sub_id;
    }

    start_deployment(deployment, runtime);

    if (deployment->lifetime_end > 0) {
      enqueue_lifetime_end_wakeup(deployment);
    }
  }

  void deactivate_deployment(Deployment* deployment) {
    decrement_deployed_actor_type_count(deployment->actor_type);
    deployment->active = false;
    stop_deployment(deployment);
  }

  void start_deployment(Deployment* deployment, Runtime* runtime) {
    Publication spawn_message{};
    spawn_message.set_attr("command", "spawn_lua_actor");

    spawn_message.set_attr("spawn_code", deployment->actor_code);
    spawn_message.set_attr("spawn_node_id", runtime->node_id);
    spawn_message.set_attr("spawn_actor_type", deployment->actor_type);
    spawn_message.set_attr("spawn_instance_id", deployment->name);

    spawn_message.set_attr("node_id", runtime->node_id);
    spawn_message.set_attr("actor_type", runtime->actor_type);
    spawn_message.set_attr("instance_id", runtime->instance_id);
    publish(std::move(spawn_message));
    deployment->restarts++;
  }

  void stop_deployment(Deployment* deployment) {
    Publication exit_message{};
    exit_message.set_attr("type", "exit");
    exit_message.set_attr("node_id", node_id());
    exit_message.set_attr("actor_type", deployment->actor_type);
    exit_message.set_attr("instance_id", deployment->name);
    publish(std::move(exit_message));
  }

  void enqueue_lifetime_end_wakeup(Deployment* deployment) {
    printf("Deployment Manager enqueue timeout %d\n",
           deployment->lifetime_end - now());
    if (auto [end_time_it, inserted] = ttl_end_times.try_emplace(
            deployment->lifetime_end, std::list<std::string>{deployment->name});
        !inserted) {
      end_time_it->second.push_back(deployment->name);
    }
    Publication timeout_message{};
    timeout_message.set_attr("type", "deployment_lifetime_end");
    timeout_message.set_attr("deployment_name", deployment->name);
    timeout_message.set_attr("node_id", node_id());
    timeout_message.set_attr("actor_type", actor_type());
    timeout_message.set_attr("instance_id", instance_id());
    delayed_publish(std::move(timeout_message),
                    deployment->lifetime_end - now() + 10);
  }

  void remove_deployment(Deployment* deployment) {
    assert(!deployment->active);

    if (deployment->lifetime_subscription_id > 0) {
      unsubscribe(deployment->lifetime_subscription_id);
      subscriptions.erase(deployment->lifetime_subscription_id);
      deployment->lifetime_subscription_id = 0;
    }

    for (auto actor_type : deployment->required_actors) {
      if (const auto requirements_it = dependencies.find(std::move(actor_type));
          requirements_it != dependencies.end()) {
        if (auto deployment_it = requirements_it->second.find(deployment);
            deployment_it != requirements_it->second.end()) {
          requirements_it->second.erase(deployment_it);
          if (requirements_it->second.size() == 0) {
            dependencies.erase(requirements_it);
          }
        }
      }
    }

    deployments.erase(deployment->name);
  }

  void add_deployment_dependencies(Deployment* deployment) {
    for (const auto& actor_type : deployment->required_actors) {
      if (auto [requirements_it, inserted] = dependencies.try_emplace(
              actor_type, std::unordered_set<Deployment*>{deployment});
          !inserted) {
        requirements_it->second.emplace(deployment);
      }
    }
  }

  void increment_deployed_actor_type_count(std::string_view actor_type) {
    auto [counter_it, inserted] =
        deployed_actor_types.try_emplace(std::string(actor_type), 1);
    counter_it != deployed_actor_types.end();
    if (!inserted) {
      counter_it->second++;
    } else {
      actor_type_added(actor_type);
    }
  }

  void decrement_deployed_actor_type_count(std::string_view actor_type) {
    if (auto counter_it = deployed_actor_types.find(std::string(actor_type));
        counter_it != deployed_actor_types.end()) {
      if (counter_it->second == 1) {
        deployed_actor_types.erase(counter_it);
        actor_type_removed(actor_type);
      } else {
        counter_it->second--;
      }
    }
  }

  void actor_type_added(std::string_view actor_type) {
    if (const auto it = dependencies.find(std::string(actor_type));
        it != dependencies.end()) {
      for (auto deployment : it->second) {
        if (requirements_check(deployment)) {
          if (const auto runtime_it =
                  runtimes.find(deployment->actor_runtime_type);
              runtime_it != runtimes.end()) {
            activate_deployment(deployment, &runtime_it->second.front());
          }
        }
      }
    }
  }

  void actor_type_removed(std::string_view actor_type) {
    if (const auto it = dependencies.find(std::string(actor_type));
        it != dependencies.end()) {
      for (auto deployment : it->second) {
        deactivate_deployment(deployment);
      }
    }
  }
};

#endif  //  MAIN_INCLUDE_DEPLOYMENT_MANAGER_HPP_
