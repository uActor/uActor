#ifndef MAIN_INCLUDE_DEPLOYMENT_MANAGER_HPP_
#define MAIN_INCLUDE_DEPLOYMENT_MANAGER_HPP_

#include <cstdint>
#include <list>
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
             std::string_view required_actors, uint32_t lifetime_end)
      : name(name),
        actor_type(actor_type),
        actor_version(actor_version),
        actor_code(actor_code),
        actor_runtime_type(actor_runtime_type),
        required_actors(required_actors),
        lifetime_end(lifetime_end) {}

  std::string name;

  std::string actor_type;
  std::string actor_version;
  std::string actor_code;
  std::string actor_runtime_type;

  std::string required_actors;

  uint32_t lifetime_subscription_id = 0;

  int32_t restarts = -1;

  uint32_t lifetime_end;
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
  std::unordered_map<uint32_t, std::string> subscriptions;

  std::list<std::pair<uint32_t, std::string>> ttl_end_times;

  uint32_t next_deployment_id = 0;
  uint32_t next_instance_id = 0;

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

        uint32_t end_time = now() + *publication.get_int_attr("deployment_ttl");

        auto [deployment_iterator, inserted] = deployments.try_emplace(
            std::string(name), name, actor_type, actor_version, actor_code,
            actor_runtime_type, required_actors, end_time);

        Deployment& deployment = deployment_iterator->second;

        if (inserted) {
          printf("Deployment Created\n");
          if (true) {  // Running all required actors
            uint32_t sub_id = subscribe(
                PubSub::Filter{PubSub::Constraint{"category", "actor_lifetime"},
                               PubSub::Constraint{"?lifetime_instance_id",
                                                  std::string(name)}});
            subscriptions.emplace(sub_id, name);
            deployment.lifetime_subscription_id = sub_id;

            start_deployment(&deployment, &runtime);

            if (*publication.get_int_attr("deployment_ttl") > 0) {
              deployment_lifetime_updated(&deployment);
            }
          } else {
            // Wait for dependencies.
          }
        } else {
          printf("Deployment Update / Refresh\n");

          if (actor_version != deployment.actor_version ||
              actor_type != deployment.actor_type) {
            stop_deployment(&deployment);
            deployment.actor_type = actor_type;
            deployment.actor_version = actor_version;
            deployment.actor_code = actor_code;
            if (auto runtime_list_it =
                    runtimes.find(deployment.actor_runtime_type);
                runtime_list_it != runtimes.end()) {
              Runtime& runtime = runtime_list_it->second.front();
              start_deployment(&deployment, &runtime);
            }
          }

          deployment.lifetime_end = end_time;
          deployment_lifetime_updated(&deployment);
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
          printf("Deployment lifetime ended \n");
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
      std::string deployment_id = ttl_end_times.begin()->second;
      if (auto deployment_it = deployments.find(deployment_id);
          deployment_it != deployments.end()) {
        Deployment& deployment = deployment_it->second;

        if (ttl_end_times.begin()->first == deployment.lifetime_end) {
          stop_deployment(&deployment);
        }
      }
      ttl_end_times.erase(ttl_end_times.begin());
    }
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

  void deployment_lifetime_updated(Deployment* deployment) {
    printf("Deployment Manager enqueue Timeout %d\n",
           deployment->lifetime_end - now());
    ttl_end_times.push_back(
        std::make_pair(deployment->lifetime_end, deployment->name));
    Publication timeout_message{};
    timeout_message.set_attr("type", "deployment_lifetime_end");
    timeout_message.set_attr("deployment_name", deployment->name);
    timeout_message.set_attr("node_id", node_id());
    timeout_message.set_attr("actor_type", actor_type());
    timeout_message.set_attr("instance_id", instance_id());
    delayed_publish(
        std::move(timeout_message),
        deployment->lifetime_end - now() + 10);
  }

  void remove_deployment(Deployment* deployment) {
    if (deployment->lifetime_subscription_id > 0) {
      unsubscribe(deployment->lifetime_subscription_id);
    }
    subscriptions.erase(deployment->lifetime_subscription_id);
    deployments.erase(deployment->name);
  }
};

#endif  //  MAIN_INCLUDE_DEPLOYMENT_MANAGER_HPP_
