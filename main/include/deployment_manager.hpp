#ifndef MAIN_INCLUDE_DEPLOYMENT_MANAGER_HPP_
#define MAIN_INCLUDE_DEPLOYMENT_MANAGER_HPP_

#include <cassert>
#include <cstdint>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "native_actor.hpp"
#include "pubsub/router.hpp"
#include "string_helper.hpp"

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
    for (std::string_view required_actor :
         StringHelper::string_split(raw_required_actors)) {
      required_actors.emplace_back(required_actor);
    }
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
};

struct Runtime {
  Runtime(std::string_view node_id, std::string_view actor_type,
          std::string_view instance_id)
      : node_id(node_id), actor_type(actor_type), instance_id(instance_id) {}
  std::string node_id;
  std::string actor_type;
  std::string instance_id;

  bool operator==(const Runtime& other) {
    return other.node_id == node_id && other.actor_type == actor_type &&
           instance_id == instance_id;
  }
};

class DeploymentManager : public NativeActor {
 public:
  DeploymentManager(ManagedNativeActor* actor_wrapper, std::string_view node_id,
                    std::string_view actor_type, std::string_view instance_id)
      : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {
    subscribe(uActor::PubSub::Filter{
        uActor::PubSub::Constraint{"node_id", std::string(node_id)},
        uActor::PubSub::Constraint{"type", "label_update"}});
    subscribe(uActor::PubSub::Filter{
        uActor::PubSub::Constraint{"node_id", std::string(node_id)},
        uActor::PubSub::Constraint{"type", "label_get"}});
    subscribe(uActor::PubSub::Filter{
        uActor::PubSub::Constraint{"node_id", std::string(node_id)},
        uActor::PubSub::Constraint{"type", "unmanaged_actor_update"}});
    subscribe(uActor::PubSub::Filter{
        uActor::PubSub::Constraint{"node_id", std::string(node_id)},
        uActor::PubSub::Constraint{"type", "runtime_update"}});
  }

  void receive(const uActor::PubSub::Publication& publication) {
    if (publication.get_str_attr("type") == "deployment") {
      receive_deployment(publication);
    } else if (publication.get_str_attr("category") == "actor_lifetime") {
      receive_lifetime_event(publication);
    } else if (publication.get_str_attr("type") == "deployment_lifetime_end") {
      receive_ttl_timeout(publication);
    } else if (publication.get_str_attr("type") == "label_update") {
      receive_label_update(publication);
    } else if (publication.get_str_attr("type") == "label_get") {
      receive_label_get(publication);
    } else if (publication.get_str_attr("type") == "unmanaged_actor_update") {
      receive_unmanaged_actor_update(publication);
    } else if (publication.get_str_attr("type") == "runtime_update") {
      receive_runtime_update(publication);
    }
  }

 private:
  std::map<std::string, std::list<Runtime>> runtimes;

  std::map<std::string, Deployment> deployments;
  std::map<uint32_t, std::string> subscriptions;
  std::map<std::string, std::set<Deployment*>> dependencies;

  std::map<std::string, std::string> labels;
  std::map<std::string, uint32_t> deployed_actor_types;

  uint32_t deployment_sub_id = 0;
  std::map<uint32_t, std::list<std::string>> ttl_end_times;

  void receive_deployment(const uActor::PubSub::Publication& publication) {
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
        if (publication.has_attr("deployment_constraints")) {
          std::string_view constraints =
              *publication.get_str_attr("deployment_constraints");
          for (std::string_view constraint :
               StringHelper::string_split(constraints)) {
            auto constraint_value = publication.get_str_attr(constraint);
            auto label = labels.find(std::string(constraint));
            if (!(constraint_value && label != labels.end() &&
                  *constraint_value == label->second)) {
              return;
            }
          }
        }

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
          printf("Receive deployment from: %s\n",
                 publication.get_str_attr("publisher_node_id")->data());
          add_deployment_dependencies(&deployment);
          if (requirements_check(&deployment)) {
            activate_deployment(&deployment, &runtime);
          }
        } else {
          printf("Receive deployment update from: %s\n",
                 publication.get_str_attr("publisher_node_id")->data());

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
        }
        if (deployment.lifetime_end > 0) {
          enqueue_lifetime_end_wakeup(&deployment);
        }
      }
    }
  }

  void receive_lifetime_event(const uActor::PubSub::Publication& publication) {
    if (publication.get_str_attr("type") == "actor_exit") {
      std::string deployment_id =
          std::string(*publication.get_str_attr("lifetime_instance_id"));
      if (const auto deployment_it = deployments.find(deployment_id);
          deployment_it != deployments.end()) {
        Deployment& deployment = deployment_it->second;
        if (deployment.lifetime_end < now()) {
          // Already deactivated
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

  void receive_ttl_timeout(const uActor::PubSub::Publication& publication) {
    while (ttl_end_times.begin() != ttl_end_times.end() &&
           ttl_end_times.begin()->first < now()) {
      for (auto deployment_id : ttl_end_times.begin()->second) {
        if (auto deployment_it = deployments.find(deployment_id);
            deployment_it != deployments.end()) {
          Deployment& deployment = deployment_it->second;

          if (ttl_end_times.begin()->first == deployment.lifetime_end) {
            bool was_active = deployment.active;
            deactivate_deployment(&deployment);
            if (!was_active) {
              remove_deployment(&deployment);
            }
          }
        }
      }
      ttl_end_times.erase(ttl_end_times.begin());
    }
  }

  void receive_label_update(const uActor::PubSub::Publication& pub) {
    auto key = pub.get_str_attr("key");
    auto value = pub.get_str_attr("value");
    if (pub.get_str_attr("command") == "upsert" && key && value) {
      labels.erase(std::string(*key));
      labels.try_emplace(std::string(*key), std::string(*value));
    } else if (pub.get_str_attr("command") == "delete" && key) {
      labels.erase(std::string(*key));
    }

    std::list<uActor::PubSub::Constraint> constraints{
        uActor::PubSub::Constraint{"type", "deployment"}};
    for (const auto label : labels) {
      constraints.emplace_back(
          label.first, label.second,
          uActor::PubSub::ConstraintPredicates::Predicate::EQ, true);
    }
    uint32_t new_deployment_sub_id =
        subscribe(uActor::PubSub::Filter{std::move(constraints)});

    if (new_deployment_sub_id != deployment_sub_id) {
      if (deployment_sub_id > 0) {
        unsubscribe(deployment_sub_id);
      }
      deployment_sub_id = new_deployment_sub_id;
    }

    uActor::PubSub::Publication node_label_updates;
    node_label_updates.set_attr("type", "label_update_notification");
    node_label_updates.set_attr("node_id", node_id());
    publish(std::move(node_label_updates));
  }

  void receive_label_get(const uActor::PubSub::Publication& pub) {
    auto key = pub.get_str_attr("key");
    auto publisher_node_id = pub.get_str_attr("publisher_node_id");
    auto publisher_actor_type = pub.get_str_attr("publisher_actor_type");
    auto publisher_instance_id = pub.get_str_attr("publisher_instance_id");

    if (!key) {
      return;
    }
    auto it = labels.find(std::string(*key));
    if (it == labels.end()) {
      return;
    }

    uActor::PubSub::Publication response{};
    response.set_attr("type", "label_response");
    response.set_attr("key", *key);
    response.set_attr("value", it->second);
    response.set_attr("node_id", *publisher_node_id);
    response.set_attr("instance_id", *publisher_instance_id);
    response.set_attr("actor_type", *publisher_actor_type);
    publish(std::move(response));
  }

  void receive_unmanaged_actor_update(const uActor::PubSub::Publication& pub) {
    if (pub.get_str_attr("command") == "register") {
      increment_deployed_actor_type_count(
          *pub.get_str_attr("update_actor_type"));
    } else if (pub.get_str_attr("command") == "deregister") {
      decrement_deployed_actor_type_count(
          *pub.get_str_attr("update_actor_type"));
    }
  }

  void receive_runtime_update(const uActor::PubSub::Publication& pub) {
    auto actor_runtime_type = pub.get_str_attr("actor_runtime_type");

    auto update_node_id = pub.get_str_attr("update_node_id");
    auto update_actor_type = pub.get_str_attr("update_actor_type");
    auto update_instance_id = pub.get_str_attr("update_instance_id");

    if (!(actor_runtime_type && update_node_id && update_actor_type &&
          update_instance_id)) {
      return;
    }

    if (pub.get_str_attr("command") == "register") {
      Runtime rt =
          Runtime(*update_node_id, *update_actor_type, *update_instance_id);
      auto [it, inserted] = runtimes.try_emplace(
          std::string(*actor_runtime_type), std::list<Runtime>{std::move(rt)});
      if (!inserted) {
        it->second.push_back(std::move(rt));
      }
    } else if (pub.get_str_attr("command") == "deregister") {
      if (auto it = runtimes.find(std::string(*actor_runtime_type));
          it != runtimes.end()) {
        Runtime comparison =
            Runtime(*update_node_id, *update_actor_type, *update_instance_id);
        auto rt_it =
            std::find(it->second.begin(), it->second.end(), comparison);
        it->second.erase(rt_it);
        if (it->second.size() == 0) {
          runtimes.erase(it);
        }
      }
    }
  }

  bool requirements_check(Deployment* deployment) {
    for (const auto& actor_type : deployment->required_actors) {
      if (deployed_actor_types.find(actor_type) == deployed_actor_types.end()) {
        return false;
      }
    }
    return true;
  }

  void activate_deployment(Deployment* deployment, Runtime* runtime) {
    printf("DeploymentManager activate deployment\n");

    increment_deployed_actor_type_count(deployment->actor_type);
    deployment->active = true;

    if (deployment->lifetime_subscription_id == 0) {
      uint32_t sub_id = subscribe(uActor::PubSub::Filter{
          uActor::PubSub::Constraint{"category", "actor_lifetime"},
          uActor::PubSub::Constraint{"lifetime_instance_id",
                                     std::string(deployment->name)},
          uActor::PubSub::Constraint{"publisher_node_id",
                                     std::string(node_id())}});
      subscriptions.try_emplace(sub_id, deployment->name);
      deployment->lifetime_subscription_id = sub_id;
    }

    start_deployment(deployment, runtime);
  }

  void deactivate_deployment(Deployment* deployment) {
    printf("DeploymentManager deactivate deployment\n");
    if (deployment->active) {
      decrement_deployed_actor_type_count(deployment->actor_type);
      deployment->active = false;
      stop_deployment(deployment);
    }
  }

  void start_deployment(Deployment* deployment, Runtime* runtime) {
    printf("DeploymentManager start deployment\n");
    uActor::PubSub::Publication spawn_message{};
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
    printf("DeploymentManager stop deployment\n");
    uActor::PubSub::Publication exit_message{};
    exit_message.set_attr("type", "exit");
    exit_message.set_attr("node_id", node_id());
    exit_message.set_attr("actor_type", deployment->actor_type);
    exit_message.set_attr("instance_id", deployment->name);
    publish(std::move(exit_message));
  }

  void enqueue_lifetime_end_wakeup(Deployment* deployment) {
    if (auto [end_time_it, inserted] = ttl_end_times.try_emplace(
            deployment->lifetime_end, std::list<std::string>{deployment->name});
        !inserted) {
      end_time_it->second.push_back(deployment->name);
    }
    uActor::PubSub::Publication timeout_message{};
    timeout_message.set_attr("type", "deployment_lifetime_end");
    timeout_message.set_attr("deployment_name", deployment->name);
    timeout_message.set_attr("node_id", node_id());
    timeout_message.set_attr("actor_type", actor_type());
    timeout_message.set_attr("instance_id", instance_id());
    delayed_publish(std::move(timeout_message),
                    deployment->lifetime_end - now() + 10);
  }

  void remove_deployment(Deployment* deployment) {
    printf("DeploymentManager remove deployment\n");
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
              actor_type, std::set<Deployment*>{deployment});
          !inserted) {
        requirements_it->second.emplace(deployment);
      }
    }
  }

  void increment_deployed_actor_type_count(std::string_view actor_type) {
    auto [counter_it, inserted] =
        deployed_actor_types.try_emplace(std::string(actor_type), 1);
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