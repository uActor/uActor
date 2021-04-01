#include "actors/http_client_actor.hpp"

#include <bits/stdint-uintn.h>
#include <curl/curl.h>
#include <curl/easy.h>

#include <cstdint>
#include <functional>
#include <iostream>
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

size_t write_function(void* ptr, size_t size, size_t nmemb, std::string* data) {
  data->append(static_cast<char*>(ptr), size * nmemb);
  return size * nmemb;
}

HTTPClientActor::HTTPClientActor()
    : handle(PubSub::Router::get_instance().new_subscriber()),
      curl(curl_easy_init()) {
  if (curl == nullptr) {
    return;
  }
  PubSub::Filter request_filter{
      PubSub::Constraint("actor_type", "http_request")};
  handle.subscribe(request_filter);
  // thread should be automatically joined/destroyed with the destructor
  this->_request_thread = std::thread(&HTTPClientActor::thread_function, this);
  // todo eventually publish availability of http_reqest actor
}

void HTTPClientActor::thread_function() {
  assert(*this);
  while (true) {
    const auto result = this->handle.receive(BoardFunctions::SLEEP_FOREVER);
    if (result.has_value()) {
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

      if (request_type.value() == "GET") {
        const std::string request_url{[&p]() {
          const auto request_url = p.get_str_attr("request_address");
          return request_url.has_value() ? request_url.value() : "";
        }()};

        std::string http_response;
        std::string http_header;
        uint8_t response_code =
            this->get_request(request_url, &http_response, &http_header);
        // todo check if name is fine
        PubSub::Publication p(BoardFunctions::NODE_ID, "core.io,http", "1");
        p.set_attr("type", "http_response");
        p.set_attr("http_header", http_header);
        p.set_attr("body", http_response);
        p.set_attr("http_code", response_code);
        p.set_attr("request_id", request_id.value());

        PubSub::Router::get_instance().publish(std::move(p));

        continue;
      }
      if (request_type.value() == "POST") {
        // todo do post request
        continue;
      }
      // todo log wrong code
    }
  }
}

uint8_t HTTPClientActor::get_request(const std::string& url,
                                     std::string* response,
                                     std::string* header) const {
  assert(*this);
  curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, header);

  const auto code = curl_easy_perform(curl);
  int64_t http_code = 0;
  if (code == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  }
  curl_easy_cleanup(curl);

  return static_cast<uint8_t>(http_code);
}

HTTPClientActor::operator bool() const { return this->curl != nullptr; }

}  // namespace uActor::HTTP
