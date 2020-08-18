#include "controllers/deployment_manager.hpp"

#include <cassert>
#include <utility>

namespace uActor::Controllers {

DeploymentManager::DeploymentManager(
    ActorRuntime::ManagedNativeActor* actor_wrapper, std::string_view node_id,
    std::string_view actor_type, std::string_view instance_id)
    : ActorRuntime::NativeActor(actor_wrapper, node_id, actor_type,
                                instance_id) {
  subscribe(PubSub::Filter{PubSub::Constraint{"node_id", std::string(node_id)},
                           PubSub::Constraint{"type", "label_update"}});
  subscribe(PubSub::Filter{PubSub::Constraint{"node_id", std::string(node_id)},
                           PubSub::Constraint{"type", "label_get"}});
  subscribe(
      PubSub::Filter{PubSub::Constraint{"node_id", std::string(node_id)},
                     PubSub::Constraint{"type", "unmanaged_actor_update"}});
  subscribe(PubSub::Filter{PubSub::Constraint{"node_id", std::string(node_id)},
                           PubSub::Constraint{"type", "executor_update"}});
}

void DeploymentManager::receive(const PubSub::Publication& publication) {
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
  } else if (publication.get_str_attr("type") == "executor_update") {
    receive_executor_update(publication);
  }
}

void DeploymentManager::receive_deployment(
    const PubSub::Publication& publication) {
  if (auto executor_it = executors.find(std::string(
          *publication.get_str_attr("deployment_actor_runtime_type")));
      executor_it != executors.end()) {
    ExecutorIdentifier& ExecutorIdentifier = executor_it->second.front();
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
             Support::StringHelper::string_split(constraints)) {
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
        publish_code_package(deployment, std::string(actor_code));
        // printf("Receive deployment from: %s\n",
        //       publication.get_str_attr("publisher_node_id")->data());
        add_deployment_dependencies(&deployment);
        if (requirements_check(&deployment)) {
          activate_deployment(&deployment, &ExecutorIdentifier);
        }
      } else {
        // printf("Receive deployment update from: %s\n",
        //       publication.get_str_attr("publisher_node_id")->data());

        if (actor_version != deployment.actor_version ||
            actor_type != deployment.actor_type) {
          stop_deployment(&deployment);
          decrement_deployed_actor_type_count(deployment.actor_type);
          deployment.actor_type = std::string(actor_type);
          deployment.actor_version = std::string(actor_version);
          publish_code_package(deployment, std::string(actor_code));
          increment_deployed_actor_type_count(deployment.actor_type);
          start_deployment(&deployment, &ExecutorIdentifier);
        }

        deployment.lifetime_end = end_time;
      }
      if (deployment.lifetime_end > 0) {
        enqueue_lifetime_end_wakeup(&deployment);
      }
    }
  }
}

void DeploymentManager::receive_lifetime_event(
    const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "actor_exit") {
    std::string deployment_id =
        std::string(*publication.get_str_attr("lifetime_instance_id"));
    if (const auto deployment_it = deployments.find(deployment_id);
        deployment_it != deployments.end()) {
      Deployment& deployment = deployment_it->second;
      if (deployment.lifetime_end > 0 && deployment.lifetime_end < now()) {
        // Already deactivated
        remove_deployment(&deployment);
      } else if (*publication.get_str_attr("exit_reason") != "clean_exit") {
        if (deployment.restarts < 3) {
          if (auto executor_list_it =
                  executors.find(deployment.actor_runtime_type);
              executor_list_it != executors.end()) {
            ExecutorIdentifier& ExecutorIdentifier =
                executor_list_it->second.front();
            start_deployment(&deployment, &ExecutorIdentifier,
                             (deployment.restarts + 1) * 1000);
          }
        } else {
          deactivate_deployment(&deployment);
          remove_deployment(&deployment);
        }
      }
    }
  }
}

void DeploymentManager::receive_ttl_timeout(
    const PubSub::Publication& publication) {
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

void DeploymentManager::receive_label_update(const PubSub::Publication& pub) {
  auto key = pub.get_str_attr("key");
  auto value = pub.get_str_attr("value");
  if (pub.get_str_attr("command") == "upsert" && key && value) {
    labels.erase(std::string(*key));
    labels.try_emplace(std::string(*key), std::string(*value));
  } else if (pub.get_str_attr("command") == "delete" && key) {
    labels.erase(std::string(*key));
  }

  std::list<PubSub::Constraint> constraints{
      PubSub::Constraint{"type", "deployment"}};
  for (const auto label : labels) {
    constraints.emplace_back(label.first, label.second,
                             PubSub::ConstraintPredicates::Predicate::EQ, true);
  }
  uint32_t new_deployment_sub_id =
      subscribe(PubSub::Filter{std::move(constraints)});

  if (new_deployment_sub_id != deployment_sub_id) {
    if (deployment_sub_id > 0) {
      unsubscribe(deployment_sub_id);
    }
    deployment_sub_id = new_deployment_sub_id;
  }

  PubSub::Publication node_label_updates;
  node_label_updates.set_attr("type", "label_update_notification");
  node_label_updates.set_attr("node_id", node_id());
  publish(std::move(node_label_updates));
}

void DeploymentManager::receive_label_get(const PubSub::Publication& pub) {
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

  PubSub::Publication response{};
  response.set_attr("type", "label_response");
  response.set_attr("key", *key);
  response.set_attr("value", it->second);
  response.set_attr("node_id", *publisher_node_id);
  response.set_attr("instance_id", *publisher_instance_id);
  response.set_attr("actor_type", *publisher_actor_type);
  publish(std::move(response));
}

void DeploymentManager::receive_unmanaged_actor_update(
    const PubSub::Publication& pub) {
  if (pub.get_str_attr("command") == "register") {
    increment_deployed_actor_type_count(*pub.get_str_attr("update_actor_type"));
  } else if (pub.get_str_attr("command") == "deregister") {
    decrement_deployed_actor_type_count(*pub.get_str_attr("update_actor_type"));
  }
}

void DeploymentManager::receive_executor_update(
    const PubSub::Publication& pub) {
  auto actor_runtime_type = pub.get_str_attr("actor_runtime_type");

  auto update_node_id = pub.get_str_attr("update_node_id");
  auto update_actor_type = pub.get_str_attr("update_actor_type");
  auto update_instance_id = pub.get_str_attr("update_instance_id");

  if (!(actor_runtime_type && update_node_id && update_actor_type &&
        update_instance_id)) {
    return;
  }

  if (pub.get_str_attr("command") == "register") {
    ExecutorIdentifier rt = ExecutorIdentifier(
        *update_node_id, *update_actor_type, *update_instance_id);
    auto [executor_it, inserted] =
        executors.try_emplace(std::string(*actor_runtime_type),
                              std::list<ExecutorIdentifier>{std::move(rt)});
    if (!inserted) {
      executor_it->second.push_back(std::move(rt));
    }
  } else if (pub.get_str_attr("command") == "deregister") {
    if (auto executor_it = executors.find(std::string(*actor_runtime_type));
        executor_it != executors.end()) {
      ExecutorIdentifier comparison = ExecutorIdentifier(
          *update_node_id, *update_actor_type, *update_instance_id);
      auto rt_it = std::find(executor_it->second.begin(),
                             executor_it->second.end(), comparison);
      executor_it->second.erase(rt_it);
      if (executor_it->second.size() == 0) {
        executors.erase(executor_it);
      }
    }
  }
}

bool DeploymentManager::requirements_check(Deployment* deployment) {
  for (const auto& actor_type : deployment->required_actors) {
    if (deployed_actor_types.find(actor_type) == deployed_actor_types.end()) {
      return false;
    }
  }
  return true;
}

void DeploymentManager::activate_deployment(
    Deployment* deployment, ExecutorIdentifier* ExecutorIdentifier) {
  // printf("DeploymentManager activate deployment\n");

  increment_deployed_actor_type_count(deployment->actor_type);
  deployment->active = true;

  if (deployment->lifetime_subscription_id == 0) {
    uint32_t sub_id = subscribe(PubSub::Filter{
        PubSub::Constraint{"category", "actor_lifetime"},
        PubSub::Constraint{"lifetime_instance_id",
                           std::string(deployment->name)},
        PubSub::Constraint{"publisher_node_id", std::string(node_id())}});
    subscriptions.try_emplace(sub_id, deployment->name);
    deployment->lifetime_subscription_id = sub_id;
  }

  start_deployment(deployment, ExecutorIdentifier);
}

void DeploymentManager::deactivate_deployment(Deployment* deployment) {
  // printf("DeploymentManager deactivate deployment\n");
  if (deployment->active) {
    decrement_deployed_actor_type_count(deployment->actor_type);
    stop_deployment(deployment);
  }
  deployment->active = false;
}

void DeploymentManager::start_deployment(Deployment* deployment,
                                         ExecutorIdentifier* ExecutorIdentifier,
                                         size_t delay) {
  // printf("DeploymentManager start deployment\n");
  PubSub::Publication spawn_message{};
  spawn_message.set_attr("command", "spawn_lua_actor");

  spawn_message.set_attr("spawn_actor_version", deployment->actor_version);
  spawn_message.set_attr("spawn_node_id", ExecutorIdentifier->node_id);
  spawn_message.set_attr("spawn_actor_type", deployment->actor_type);
  spawn_message.set_attr("spawn_instance_id", deployment->name);

  spawn_message.set_attr("node_id", ExecutorIdentifier->node_id);
  spawn_message.set_attr("actor_type", ExecutorIdentifier->actor_type);
  spawn_message.set_attr("instance_id", ExecutorIdentifier->instance_id);
  delayed_publish(std::move(spawn_message), delay);
  deployment->restarts++;
}

void DeploymentManager::stop_deployment(Deployment* deployment) {
  // printf("DeploymentManager stop deployment\n");
  PubSub::Publication exit_message{};
  exit_message.set_attr("type", "exit");
  exit_message.set_attr("node_id", node_id());
  exit_message.set_attr("actor_type", deployment->actor_type);
  exit_message.set_attr("instance_id", deployment->name);
  publish(std::move(exit_message));
}

void DeploymentManager::enqueue_lifetime_end_wakeup(Deployment* deployment) {
  if (auto [end_time_it, inserted] = ttl_end_times.try_emplace(
          deployment->lifetime_end, std::list<std::string>{deployment->name});
      !inserted) {
    end_time_it->second.push_back(deployment->name);
  }
  PubSub::Publication timeout_message{};
  timeout_message.set_attr("type", "deployment_lifetime_end");
  timeout_message.set_attr("deployment_name", deployment->name);
  timeout_message.set_attr("node_id", node_id());
  timeout_message.set_attr("actor_type", actor_type());
  timeout_message.set_attr("instance_id", instance_id());
  delayed_publish(std::move(timeout_message),
                  deployment->lifetime_end - now() + 10);
}

void DeploymentManager::remove_deployment(Deployment* deployment) {
  // printf("DeploymentManager remove deployment\n");
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

void DeploymentManager::add_deployment_dependencies(Deployment* deployment) {
  for (const auto& actor_type : deployment->required_actors) {
    if (auto [requirements_it, inserted] = dependencies.try_emplace(
            actor_type, std::set<Deployment*>{deployment});
        !inserted) {
      requirements_it->second.emplace(deployment);
    }
  }
}

void DeploymentManager::increment_deployed_actor_type_count(
    std::string_view actor_type) {
  auto [counter_it, inserted] =
      deployed_actor_types.try_emplace(std::string(actor_type), 1);
  if (!inserted) {
    counter_it->second++;
  } else {
    actor_type_added(actor_type);
  }
}

void DeploymentManager::decrement_deployed_actor_type_count(
    std::string_view actor_type) {
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

void DeploymentManager::actor_type_added(std::string_view actor_type) {
  if (const auto it = dependencies.find(std::string(actor_type));
      it != dependencies.end()) {
    for (auto deployment : it->second) {
      if (requirements_check(deployment)) {
        if (const auto executor_it =
                executors.find(deployment->actor_runtime_type);
            executor_it != executors.end()) {
          activate_deployment(deployment, &executor_it->second.front());
        }
      }
    }
  }
}

void DeploymentManager::actor_type_removed(std::string_view actor_type) {
  if (const auto it = dependencies.find(std::string(actor_type));
      it != dependencies.end()) {
    for (auto deployment : it->second) {
      deactivate_deployment(deployment);
    }
  }
}

void DeploymentManager::publish_code_package(const Deployment& deployment,
                                             std::string&& code) {
  PubSub::Publication actor_code_message(node_id(), actor_type(),
                                         instance_id());
  actor_code_message.set_attr("type", "actor_code");
  actor_code_message.set_attr("actor_code_type", deployment.actor_type);
  actor_code_message.set_attr("actor_code_version", deployment.actor_version);
  actor_code_message.set_attr("actor_code_runtime_type",
                              deployment.actor_runtime_type);
  actor_code_message.set_attr("actor_code_lifetime_end",
                              static_cast<int32_t>(deployment.lifetime_end));
  actor_code_message.set_attr("actor_code", std::move(code));
  publish(std::move(actor_code_message));
}

}  // namespace uActor::Controllers
