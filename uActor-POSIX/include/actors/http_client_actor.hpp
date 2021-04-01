#pragma once

#include <cstdint>
#include <string>
#include <thread>

#include "pubsub/receiver_handle.hpp"

namespace uActor::HTTP {

class HTTPClientActor {
 private:
  PubSub::ReceiverHandle handle;
  std::thread _request_thread;
  void* curl;

 public:
  HTTPClientActor();
  // Not copyable, will lead to weird behaviour with thread
  HTTPClientActor(const HTTPClientActor& other) = delete;
  HTTPClientActor(HTTPClientActor&& other) = default;

  // todo if announce deannounce HTTPClient Actor
  ~HTTPClientActor() = default;
  // todo check with raphael if it is ok to allow non const refs
  [[nodiscard]] uint8_t get_request(const std::string& url,
                                    std::string* response,
                                    std::string* header) const;
  void thread_function();
  // returns if HTTPClientActor is functional
  explicit operator bool() const;

  HTTPClientActor& operator=(const HTTPClientActor& other) = delete;
  HTTPClientActor& operator=(HTTPClientActor&& other) = default;
};

}  // namespace uActor::HTTP
