#ifndef UACTOR_INCLUDE_PUBSUB_VECTOR_BUFFER_HPP_
#define UACTOR_INCLUDE_PUBSUB_VECTOR_BUFFER_HPP_

#include <memory>
#include <utility>
#include <vector>

namespace uActor::PubSub {

class VectorBuffer {
 public:
  void write(const char* buf, size_t len) {
    buffer->insert(buffer->end(), buf, buf + len);
  }

  std::shared_ptr<std::vector<char>> fetch_buffer() {
    return std::move(buffer);
  }

 private:
  std::shared_ptr<std::vector<char>> buffer =
      std::make_shared<std::vector<char>>(4);
};
}  // namespace uActor::PubSub

#endif  // UACTOR_INCLUDE_PUBSUB_VECTOR_BUFFER_HPP_
