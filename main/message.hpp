#ifndef MAIN_MESSAGE_HPP_
#define MAIN_MESSAGE_HPP_

#include <cstdint>

#include "benchmark_configuration.hpp"

class Message {
 public:
  Message(const Message& old) = delete;
  Message(Message&& old) : _data(old._data) { old._data = nullptr; }

  Message& operator=(Message&& old) {
    if (_data) {
      delete _data;
    }
    _data = old._data;
    old._data = nullptr;
    return *this;
  }

  explicit Message(size_t size, bool init = true) {
    if (init) {
      _data = new char[size + 2 * sizeof(uint64_t) + sizeof(size_t)];
      // assert(_data);
      *reinterpret_cast<size_t*>(_data + 2 * sizeof(uint64_t)) = size;
    } else {
      _data = nullptr;
    }
  }

  ~Message() { delete _data; }

  void moved() { _data = nullptr; }

  uint64_t sender() const { return *reinterpret_cast<uint64_t*>(_data); }

  uint64_t receiver() const {
    return *(reinterpret_cast<uint64_t*>(_data) + 1);
  }

  size_t size() const {
    return *reinterpret_cast<size_t*>(_data + 2 * sizeof(uint64_t));
  }

  void sender(uint64_t sender) { *reinterpret_cast<uint64_t*>(_data) = sender; }

  void receiver(uint64_t receiver) {
    *(reinterpret_cast<uint64_t*>(_data) + 1) = receiver;
  }

  char* buffer() { return _data + 2 * sizeof(uint64_t) + sizeof(size_t); }

  void* raw() { return _data; }

 private:
  char* _data;
};

#endif  //  MAIN_MESSAGE_HPP_
