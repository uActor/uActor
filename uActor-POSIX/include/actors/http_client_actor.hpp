#pragma once

#include <cstdint>
#include <string>
#include <thread>

#include "pubsub/matched_publication.hpp"
#include "pubsub/publication.hpp"
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

  ~HTTPClientActor();

  HTTPClientActor& operator=(const HTTPClientActor& other) = delete;
  HTTPClientActor& operator=(HTTPClientActor&& other) = default;

 private:
  void thread_function();

  static void handle_publication(PubSub::Publication&& p);

  [[nodiscard]] static inline curl_slist* build_header(
      const std::optional<std::string>& request_header);
  [[nodiscard]] static inline uint8_t perform_request(void* curl);
  [[nodiscard]] static uint8_t get_request(
      const std::string& url, const std::optional<std::string>& request_header,
      std::string* request_payload);
  [[nodiscard]] static uint8_t post_request(
      const std::string& url, const std::optional<std::string>& request_header,
      const std::string& payload, std::string* response_payload);
};

}  // namespace uActor::HTTP
