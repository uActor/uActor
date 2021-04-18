#include "actors/http_client_actor.hpp"

#include <bits/stdint-intn.h>
#include <bits/stdint-uintn.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/options.h>
#include <fmt/compile.h>
#include <fmt/core.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "board_functions.hpp"
#include "pubsub/constraint.hpp"
#include "pubsub/filter.hpp"
#include "pubsub/publication.hpp"
#include "pubsub/receiver_handle.hpp"
#include "pubsub/router.hpp"
#include "support/testbed.h"

namespace uActor::HTTP {

constexpr std::string_view PUBLICATION_NAME("core.io.http");

size_t write_function(void* ptr, size_t size, size_t nmemb, std::string* data) {
  data->append(static_cast<char*>(ptr), size * nmemb);
  return size * nmemb;
}

HTTPClientActor::HTTPClientActor()
    : handle(PubSub::Router::get_instance().new_subscriber()) {
  PubSub::Filter request_filter{
      PubSub::Constraint("actor_type", "http_request")};
  handle.subscribe(request_filter);
  // thread should be automatically joined/destroyed with the destructor
  this->_request_thread = std::thread(&HTTPClientActor::thread_function, this);
  // todo eventually publish availability of http_reqest actor
}

void HTTPClientActor::thread_function() {
  while (true) {
    const auto result = this->handle.receive(BoardFunctions::SLEEP_FOREVER);
    if (!result.has_value()) {
      continue;
    }
    const auto& p = result->publication;
    assert("http_reqest" == p.get_str_attr("actor_type"));
    auto request_type = p.get_str_attr("http_method");
    if (!request_type.has_value()) {
      continue;
    }

    auto request_id = p.get_int_attr("request_id");
    if (!request_id.has_value()) {
      continue;
    }

    const std::string request_url{[&p]() {
      const auto request_url = p.get_str_attr("request_address");
      return request_url.has_value() ? request_url.value() : "";
    }()};

    std::optional<std::string> request_header{[&p]() {
      const auto request_header = p.get_str_attr("headers");
      return request_header.has_value()
                 ? std::make_optional(request_header.value())
                 : std::nullopt;
    }()};

    if (request_type.value() == "GET") {
      std::string http_response;
      std::string http_header;
      uint8_t response_code = this->get_request(request_url, request_header,
                                                &http_response, &http_header);
      // todo check if name is fine
      PubSub::Publication p(BoardFunctions::NODE_ID, PUBLICATION_NAME, "1");
      p.set_attr("type", "http_response");
      p.set_attr("http_header", std::move(http_header));
      p.set_attr("body", std::move(http_response));
      p.set_attr("http_code", response_code);
      p.set_attr("request_id", request_id.value());

      PubSub::Router::get_instance().publish(std::move(p));

      continue;
    }
    if (request_type.value() == "POST") {
      std::string request_payload;
      if (p.has_attr("attributes")) {
        const std::string_view attrs = p.get_str_attr("attributes").value();
        size_t last_hit = 0;
        for (size_t i = 0; i < attrs.size(); i++) {
          if (attrs[i] != ',') {
            // remove leading spaces
            if (last_hit == i && attrs[i] == ' ') {
              last_hit = i + 1;
            }
            continue;
          }

          if (last_hit == i) {
            last_hit = i + 1;
            continue;
          }

          std::string_view attr = attrs.substr(last_hit, i - last_hit);
          last_hit = i + 1;

          // remove tailing spaces
          while (!attr.empty() && attr.back() == ' ') {
            attr.remove_suffix(1);
          }

          if (attr.empty()) {
            continue;
          }

          const auto value = p.get_str_attr(attr);
          if (value.has_value()) {
            request_payload = fmt::format(FMT_COMPILE("{}&{}={}"),
                                          request_payload, attr, value.value());
          } else {
            request_payload =
                fmt::format(FMT_COMPILE("{}&{}"), request_payload, attr);
          }
        }
      } else if (p.has_attr("body")) {
        request_payload = p.get_str_attr("body").value();
      }

      uint8_t response_code =
          post_request(request_url, request_header, request_payload);

      PubSub::Publication p(BoardFunctions::NODE_ID, PUBLICATION_NAME, "1");
      p.set_attr("type", "http_response");
      p.set_attr("http_code", response_code);
      p.set_attr("request_id", request_id.value());

      PubSub::Router::get_instance().publish(std::move(p));
      continue;
    }
    // todo log wrong code
  }
}

void HTTPClientActor::prep_request(const std::string& url, void* curl,
                                   curl_slist* request_header) const {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  if (request_header != nullptr) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_header);
  }
}

curl_slist* HTTPClientActor::build_header(
    const std::optional<std::string>& request_header) const {
  if (!request_header.has_value()) {
    return nullptr;
  }
  return curl_slist_append(nullptr, request_header->c_str());
}

uint8_t HTTPClientActor::perform_request(void* curl,
                                         curl_slist* request_header) const {
  const CURLcode code = curl_easy_perform(curl);
  int64_t http_code = 0;
  if (code == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  }
  curl_easy_cleanup(curl);
  curl_slist_free_all(request_header);
  return static_cast<uint8_t>(http_code);
}

uint8_t HTTPClientActor::get_request(
    const std::string& url, const std::optional<std::string>& request_header,
    std::string* response_payload, std::string* resp_header) const {
  void* curl = curl_easy_init();
  struct curl_slist* request_header_list = build_header(request_header);
  this->prep_request(url, curl, request_header_list);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_payload);
  if (resp_header != nullptr) {
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, resp_header);
  }
  return this->perform_request(curl, request_header_list);
}

uint8_t HTTPClientActor::post_request(
    const std::string& url, const std::optional<std::string>& request_header,
    const std::string& payload) const {
  void* curl = curl_easy_init();
  curl_slist* request_header_list = build_header(request_header);
  this->prep_request(url, curl, request_header_list);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
  return this->perform_request(curl, request_header_list);
}
}  // namespace uActor::HTTP
