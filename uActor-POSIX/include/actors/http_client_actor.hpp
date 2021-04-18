#pragma once

#include <cstdint>
#include <string>
#include <thread>

#include "pubsub/receiver_handle.hpp"

struct curl_slist;

namespace uActor::HTTP {

class HTTPClientActor {
 private:
  PubSub::ReceiverHandle handle;
  std::thread _request_thread;

 public:
  HTTPClientActor();
  // Not copyable, will lead to weird behaviour with thread
  HTTPClientActor(const HTTPClientActor& other) = delete;
  HTTPClientActor(HTTPClientActor&& other) = default;

  // todo if announce deannounce HTTPClient Actor
  ~HTTPClientActor() = default;
  // todo check with raphael if it is ok to allow non const refs
  void thread_function();

  HTTPClientActor& operator=(const HTTPClientActor& other) = delete;
  HTTPClientActor& operator=(HTTPClientActor&& other) = default;

 private:
  inline void prep_request(const std::string& url, void* curl,
                           curl_slist* request_header) const;
  [[nodiscard]] inline curl_slist* build_header(
      const std::optional<std::string>& request_header) const;
  [[nodiscard]] inline uint8_t perform_request(
      void* curl, curl_slist* request_header) const;
  [[nodiscard]] uint8_t get_request(
      const std::string& url, const std::optional<std::string>& request_header,
      std::string* response_payload, std::string* resp_header) const;
  [[nodiscard]] uint8_t post_request(
      const std::string& url, const std::optional<std::string>& request_header,
      const std::string& payload) const;
};

}  // namespace uActor::HTTP
