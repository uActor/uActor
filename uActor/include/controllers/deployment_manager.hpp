#ifndef UACTOR_INCLUDE_CONTROLLERS_DEPLOYMENT_MANAGER_HPP_
#define UACTOR_INCLUDE_CONTROLLERS_DEPLOYMENT_MANAGER_HPP_

#include <cstdint>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "actor_runtime/code_store.hpp"
#include "actor_runtime/native_actor.hpp"
#include "pubsub/router.hpp"
#include "support/string_helper.hpp"

namespace uActor::Controllers {

class DeploymentManager : public ActorRuntime::NativeActor {
 public:
  DeploymentManager(ActorRuntime::ManagedNativeActor* actor_wrapper,
                    std::string_view node_id, std::string_view actor_type,
                    std::string_view instance_id);

  void receive(const PubSub::Publication& publication);

 private:
  struct Deployment {
    Deployment(std::string_view name, std::string_view actor_type,
               std::string_view actor_version, std::string_view actor_code,
               std::string_view actor_runtime_type,
               std::string_view raw_required_actors, uint32_t lifetime_end)
        : name(name),
          actor_type(actor_type),
          actor_version(actor_version),
          actor_runtime_type(actor_runtime_type),
          lifetime_end(lifetime_end) {
      for (std::string_view required_actor :
           Support::StringHelper::string_split(raw_required_actors)) {
        required_actors.emplace_back(required_actor);
      }
    }

    std::string name;

    std::string actor_type;
    std::string actor_version;
    std::string actor_runtime_type;

    std::list<std::string> required_actors;

    uint32_t lifetime_subscription_id = 0;
    int32_t restarts = -1;
    uint32_t lifetime_end;
    bool active = false;
  };

  struct ExecutorIdentifier {
    ExecutorIdentifier(std::string_view node_id, std::string_view actor_type,
                       std::string_view instance_id)
        : node_id(node_id), actor_type(actor_type), instance_id(instance_id) {}
    std::string node_id;
    std::string actor_type;
    std::string instance_id;

    bool operator==(const ExecutorIdentifier& other) {
      return other.node_id == node_id && other.actor_type == actor_type &&
             instance_id == instance_id;
    }
  };

  std::map<std::string, std::list<ExecutorIdentifier>> executors;

  std::map<std::string, Deployment> deployments;
  std::map<uint32_t, std::string> subscriptions;
  std::map<std::string, std::set<Deployment*>> dependencies;

  std::map<std::string, std::string> labels;
  std::map<std::string, uint32_t> deployed_actor_types;

  uint32_t deployment_sub_id = 0;
  std::map<uint32_t, std::list<std::string>> ttl_end_times;

  void receive_deployment(const PubSub::Publication& publication);

  void receive_lifetime_event(const PubSub::Publication& publication);

  void receive_ttl_timeout(const PubSub::Publication& publication);

  void receive_label_update(const PubSub::Publication& pub);

  void receive_label_get(const PubSub::Publication& pub);

  void receive_unmanaged_actor_update(const PubSub::Publication& pub);

  void receive_executor_update(const PubSub::Publication& pub);

  bool requirements_check(Deployment* deployment);

  void activate_deployment(Deployment* deployment,
                           ExecutorIdentifier* ExecutorIdentifier);

  void deactivate_deployment(Deployment* deployment);

  void start_deployment(Deployment* deployment,
                        ExecutorIdentifier* ExecutorIdentifier,
                        size_t delay = 0);

  void stop_deployment(Deployment* deployment);

  void enqueue_lifetime_end_wakeup(Deployment* deployment);

  void remove_deployment(Deployment* deployment);

  void add_deployment_dependencies(Deployment* deployment);

  void increment_deployed_actor_type_count(std::string_view actor_type);

  void decrement_deployed_actor_type_count(std::string_view actor_type);

  void actor_type_added(std::string_view actor_type);

  void actor_type_removed(std::string_view actor_type);

  void publish_code_package(const Deployment& deployment, std::string&& code);
};

}  // namespace uActor::Controllers

#endif  //  UACTOR_INCLUDE_CONTROLLERS_DEPLOYMENT_MANAGER_HPP_
