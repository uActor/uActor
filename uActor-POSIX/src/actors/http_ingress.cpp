#if CONFIG_UACTOR_ENABLE_HTTP_INGRESS
#include "actors/http_ingress.hpp"

#include "pubsub/router.hpp"
#include "support/logger.hpp"
#include "support/string_helper.hpp"

namespace uActor::Ingress {
void HTTPIngress::receive(const PubSub::Publication& publication) {
  if (publication.get_str_attr("type") == "init") {
    server_thread = std::thread(&server, this, 3030);
    add_subscription_update_subscriptions();
    enqueue_wakeup(5000, "periodic_cleanup");
  } else if (publication.get_str_attr("type") == "exit") {
    stop_server();
  } else if (publication.get_str_attr("type") == "ingress_http_response") {
    handle_http_response(publication);
  } else if (publication.get_str_attr("type") == "local_subscription_added" ||
             publication.get_str_attr("type") == "local_subscription_exists") {
    auto subscription = parse_http_ingress_subscription(publication);
    if (subscription) {
      handle_subscription_added(std::move(*subscription));
    }
  } else if (publication.get_str_attr("type") == "local_subscription_removed") {
    auto subscription = parse_http_ingress_subscription(publication);
    if (subscription) {
      handle_subscription_removed(std::move(*subscription));
    }
  } else if (publication.get_str_attr("type") == "wakeup" &&
             publication.get_str_attr("wakeup_id") == "periodic_cleanup") {
    periodic_cleanup();
    enqueue_wakeup(5000, "periodic_cleanup");
  } else {
    Support::Logger::warning("HTTP-INGRESS", "Received unknown message.");
  }
}

void HTTPIngress::handle_http_response(const PubSub::Publication& publication) {
  if (publication.has_attr("http_request_id")) {
    size_t request_id = *publication.get_int_attr("http_request_id");
    std::string body;
    if (publication.has_attr("http_body")) {
      body = *publication.get_str_attr("http_body");
    }
    auto ar_i = active_requests.find(request_id);
    if (ar_i != active_requests.end()) {
      if (publication.has_attr("http_headers")) {
        for (const auto& key : Support::StringHelper::string_split(
                 *publication.get_str_attr("http_headers"), ";")) {
          if (publication.has_attr(key)) {
            auto val = *publication.get_str_attr(key);
            ar_i->second.res->writeHeader(key, val);
            ar_i->second.res->end(body);
          }
        }
      } else {
        ar_i->second.res->end(body);
      }
      active_requests.erase(request_id);
    }
  }
}

void HTTPIngress::periodic_cleanup() {
  std::vector<size_t> to_delete;
  auto cutoff = std::chrono::system_clock::now();

  for (const auto& [request_id, request] : active_requests) {
    if ((request.start_time + std::chrono::seconds(5)) < cutoff) {
      Support::Logger::warning("HTTP-INGRESS", "Request timed out.");
      request.res->writeStatus("504");
      request.res->end();
      to_delete.push_back(request_id);
    }
  }

  for (auto item : to_delete) {
    active_requests.erase(item);
  }
}

// Subscription Tracking --- allows to return 404 for URLs without any active
// handler.

void HTTPIngress::add_subscription_update_subscriptions() {
  subscribe(
      PubSub::Filter{PubSub::Constraint("type", "local_subscription_added"),
                     PubSub::Constraint("publisher_node_id",
                                        std::string(BoardFunctions::NODE_ID))});

  subscribe(
      PubSub::Filter{PubSub::Constraint("type", "local_subscription_removed"),
                     PubSub::Constraint("publisher_node_id",
                                        std::string(BoardFunctions::NODE_ID))});
}

void HTTPIngress::handle_subscription_added(
    SubscriptionIdentifier&& subscription_arguments) {
  Support::Logger::info(
      "HTTP-INGRESS", "Subscription added: %s %s %s",
      subscription_arguments.method.c_str(), subscription_arguments.url.c_str(),
      PubSub::ConstraintPredicates::name(subscription_arguments.predicate));
  auto [iterator, inserted] =
      active_subscriptions.try_emplace(subscription_arguments, 1);
  if (!inserted) {
    iterator->second++;
  }
}

void HTTPIngress::handle_subscription_removed(
    SubscriptionIdentifier&& subscription_arguments) {
  auto sub_iterator = active_subscriptions.find(subscription_arguments);
  if (sub_iterator != active_subscriptions.end()) {
    if (sub_iterator->second <= 1) {
      active_subscriptions.erase(sub_iterator);
    } else {
      sub_iterator->second--;
    }
  }
}

std::optional<HTTPIngress::SubscriptionIdentifier>
HTTPIngress::parse_http_ingress_subscription(
    const PubSub::Publication& publication) {
  if (!publication.has_attr("subscription")) {
    return std::nullopt;
  }

  auto map = *publication.get_nested_component("subscription");

  auto deserialized_subscription = PubSub::Filter::from_publication_map(*map);

  if (!(deserialized_subscription &&
        filter_has_ingress_http_constraint(*deserialized_subscription))) {
    return std::nullopt;
  }

  SubscriptionIdentifier request_identifier{};

  for (const auto& c : deserialized_subscription->required_constraints()) {
    if (c.attribute() == "http_method" &&
        std::holds_alternative<std::string_view>(c.operand())) {
      request_identifier.method = std::get<std::string_view>(c.operand());
    }
    if (c.attribute() == "http_url" &&
        std::holds_alternative<std::string_view>(c.operand())) {
      request_identifier.url = std::get<std::string_view>(c.operand());
      request_identifier.predicate = c.predicate();
    }
  }

  if (request_identifier.method.length() > 0 &&
      request_identifier.url.length() > 0) {
    return std::move(request_identifier);
  } else {
    return std::nullopt;
  }
}

bool HTTPIngress::filter_has_ingress_http_constraint(
    const PubSub::Filter& filter) {
  auto is_ingress_http_request_constraint = [](const auto& constraint) {
    return constraint.attribute() == "type" &&
           std::holds_alternative<std::string_view>(constraint.operand()) &&
           std::get<std::string_view>(constraint.operand()) ==
               "ingress_http_request";
  };

  return std::find_if(filter.required_constraints().begin(),
                      filter.required_constraints().end(),
                      is_ingress_http_request_constraint) !=
         filter.required_constraints().end();
}

// Main HTTP Server

void HTTPIngress::server(HTTPIngress* ingress, size_t port) {
  uWS::App()
      .any("/*", [ingress](auto* res,
                           auto* req) { ingress->handle_request(res, req); })
      .listen(port,
              [port, ingress](auto* ls) {
                if (ls) {
                  ingress->listen_socket = ls;
                  Support::Logger::info("HTTP-INGRESS",
                                        "HTTP Ingress listening on port %d",
                                        port);
                }
              })
      .run();
  Support::Logger::info("HTTP-INGRESS", "Server end");
}

void HTTPIngress::stop_server() {
  if (server_thread.joinable() && listen_socket) {
    // This seems to be the only way to stop uWS:
    // https://stackoverflow.com/a/59747314
    us_listen_socket_close(0, listen_socket);
    listen_socket = nullptr;
    active_requests.clear();
    server_thread.join();
  }
}

void HTTPIngress::handle_request(uWS::HttpResponse<false>* res,
                                 uWS::HttpRequest* req) {
  if (!matches_active_subscription(req)) {
    res->writeStatus("404");
    res->end();
    return;
  }

  int32_t id = next_id++;

  active_requests.try_emplace(id, ActiveRequest(*req, res));

  res->onData([this, id](std::string_view chunk, bool is_last) {
    this->handle_data(id, chunk, is_last);
  });

  res->onAborted([this, id]() { this->active_requests.erase(id); });
}

bool HTTPIngress::matches_active_subscription(uWS::HttpRequest* req) {
  return (
      std::find_if(active_subscriptions.begin(), active_subscriptions.end(),
                   [req](const auto& item) {
                     const auto& sub_id = item.first;
                     return req->getMethod() == sub_id.method &&
                            ((sub_id.predicate ==
                                  PubSub::ConstraintPredicates::Predicate::EQ &&
                              req->getUrl() == sub_id.url) ||
                             (sub_id.predicate ==
                                  PubSub::ConstraintPredicates::Predicate::PF &&
                              req->getUrl().find(sub_id.url) == 0));
                   }) != active_subscriptions.end());
}

void HTTPIngress::handle_data(int32_t request_id, std::string_view chunk,
                              bool is_last) {
  auto ar_i = active_requests.find(request_id);
  if (ar_i != active_requests.end()) {
    auto& request = ar_i->second;
    request.body += chunk;
    if (is_last) {
      request.fully_received = true;
      PubSub::Publication p{};
      p.set_attr("type", "ingress_http_request");
      p.set_attr("http_method", request.req.getMethod());
      p.set_attr("http_body", request.body);
      p.set_attr("http_url", request.req.getUrl());
      p.set_attr("http_request_id", request_id);

      if (request.req.getQuery().size() > 0) {
        p.set_attr("http_query", request.req.getQuery());
      }

      std::string header_keys;
      for (const auto& [header_key, header_value] : request.req) {
        header_keys +=
            std::string(header_key) + (header_keys.size() > 0 ? ";" : "");
        p.set_attr(header_key, header_value);
      }
      if (header_keys.size() > 0) {
        p.set_attr("http_headers", header_keys);
      }

      publish(std::move(p));
    }
  }
}
}  // namespace uActor::Ingress
#endif
