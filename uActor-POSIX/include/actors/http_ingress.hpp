#pragma once

#include <App.h>

#include <map>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "actor_runtime/native_actor.hpp"

namespace uActor::Ingress {
class HTTPIngress : public ActorRuntime::NativeActor {
 public:
  HTTPIngress(ActorRuntime::ManagedNativeActor* actor_wrapper,
              std::string_view node_id, std::string_view actor_type,
              std::string_view instance_id)
      : NativeActor(actor_wrapper, node_id, actor_type, instance_id) {}

  ~HTTPIngress() = default;

  void receive(const PubSub::Publication& publication);

 private:
  struct ActiveRequest {
    ActiveRequest(uWS::HttpRequest req, uWS::HttpResponse<false>* res)
        : req(std::move(req)), res(res) {}
    uWS::HttpRequest req;
    uWS::HttpResponse<false>* res;
    std::string body;
    bool fully_received = false;
    std::chrono::time_point<std::chrono::system_clock> start_time =
        std::chrono::system_clock::now();
  };

  struct SubscriptionIdentifier {
    std::string method;
    std::string url;
    PubSub::ConstraintPredicates::Predicate predicate;

    SubscriptionIdentifier(std::string&& method, std::string&& url,
                           PubSub::ConstraintPredicates::Predicate predicate)
        : method(std::move(method)),
          url(std::move(url)),
          predicate(predicate) {}

    SubscriptionIdentifier()
        : predicate(PubSub::ConstraintPredicates::Predicate::EQ) {}

    bool operator<(const SubscriptionIdentifier& other) const {
      return method < other.method ||
             (method == other.method &&
              (url < other.url ||
               (url == other.url && predicate < other.predicate)));
    }
  };

  std::atomic<size_t> next_id{1};

  std::thread server_thread;
  std::map<size_t, ActiveRequest> active_requests;

  std::map<SubscriptionIdentifier, size_t> active_subscriptions;

  us_listen_socket_t* listen_socket;

  void handle_http_response(const PubSub::Publication& publication);

  void periodic_cleanup();

  // Subscription Tracking --- allows to return 404 for URLs without any active
  // handler.

  void add_subscription_update_subscriptions();

  void handle_subscription_added(
      SubscriptionIdentifier&& subscription_arguments);
  void handle_subscription_removed(
      SubscriptionIdentifier&& subscription_arguments);

  std::optional<HTTPIngress::SubscriptionIdentifier>
  parse_http_ingress_subscription(const PubSub::Publication& publication);
  bool filter_has_ingress_http_constraint(const PubSub::Filter& filter);

  // Main HTTP Server

  static void server(HTTPIngress* ingress, size_t port);
  void stop_server();
  void handle_request(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
  bool matches_active_subscription(uWS::HttpRequest* req);
  void handle_data(int32_t request_id, std::string_view chunk, bool is_last);
};
}  // namespace uActor::Ingress
