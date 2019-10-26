#ifndef MAIN_MESSAGE_HPP_
#define MAIN_MESSAGE_HPP_

#include <cstdint>

#include "benchmark_configuration.hpp"

class Message {
 public:
  Message(const Message& old) = delete;
  Message(Message&& old) : data_(old.data_) { old.data_ = nullptr; }

  Message& operator=(Message&& old) {
    if (data_) {
      delete data_;
    }
    data_ = old.data_;
    old.data_ = nullptr;
    return *this;
  }

  explicit Message(bool init = true) {
    if (init) {
      data_ = new char[STATIC_MESSAGE_SIZE + 16];
    } else {
      data_ = nullptr;
    }
  }

  ~Message() { delete data_; }

  void moved() { data_ = nullptr; }

  uint64_t sender() { return *reinterpret_cast<uint64_t*>(data_); }

  uint64_t receiver() { return *(reinterpret_cast<uint64_t*>(data_) + 1); }

  void sender(uint64_t sender) { *reinterpret_cast<uint64_t*>(data_) = sender; }

  void receiver(uint64_t receiver) {
    *(reinterpret_cast<uint64_t*>(data_) + 1) = receiver;
  }

  char* buffer() { return data_ + 16; }

  void* raw() { return data_; }

 private:
  char* data_;
};

#endif  //  MAIN_MESSAGE_HPP_
