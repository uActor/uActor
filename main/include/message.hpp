#ifndef MAIN_INCLUDE_MESSAGE_HPP_
#define MAIN_INCLUDE_MESSAGE_HPP_

#include <cassert>
#include <cstdint>
#include <cstring>

#include "../benchmark_configuration.hpp"

struct Tags {
  enum WELL_KNOWN_TAGS : uint32_t {
    INIT = 1,
    SPAWN_LUA_ACTOR,
    TIMEOUT,
    EXIT,
    FAKE_ITERATOR_END  // well known last value to allow iterator over the enum
  };
  constexpr static const char* tag_name(uint32_t tag) {
    switch (tag) {
      case 1:
        return "init";
      case 2:
        return "spawn_lua_actor";
      case 3:
        return "timeout";
      case 4:
        return "exit";
      case 5:  // every value requested should be defined, hence no default
        return nullptr;
    }
    return nullptr;
  }
};

class Message {
 public:
  Message(const Message& old) : _data(nullptr) {
    _data = reinterpret_cast<InternalDataStructure*>(
        new char[old.payload_size() + InternalDataStructure::size_overhead]);
    std::memcpy(_data, old._data,
                old.payload_size() + InternalDataStructure::size_overhead);
  }

  Message& operator=(const Message& old) {
    if (_data && old.payload_size() != payload_size()) {
      delete reinterpret_cast<char*>(_data);
    }
    std::memcpy(_data, old._data,
                old.payload_size() + InternalDataStructure::size_overhead);
    return *this;
  }

  Message(Message&& old) : _data(old._data) { old._data = nullptr; }

  Message& operator=(Message&& old) {
    if (_data) {
      delete reinterpret_cast<char*>(_data);
    }
    _data = old._data;
    old._data = nullptr;
    return *this;
  }

  Message() { _data = nullptr; }

  Message(const char* sender, const char* receiver, uint32_t tag,
          const char* message, size_t message_size = 0) {
    size_t buffer_size = message_size > 0 ? message_size : strlen(message) + 1;
    size_t sender_size = strlen(sender) + 1;
    size_t receiver_size = strlen(receiver) + 1;
    size_t payload_size = sender_size + buffer_size + receiver_size;
    _data = reinterpret_cast<InternalDataStructure*>(
        new char[payload_size + InternalDataStructure::size_overhead]);

    _data->sender_size = sender_size;
    _data->receiver_size = receiver_size;
    _data->buffer_size = buffer_size;
    _data->tag = tag;

    std::memcpy(_data->payload, sender, sender_size);
    std::memcpy(_data->payload + sender_size, receiver, receiver_size);
    std::memcpy(_data->payload + sender_size + receiver_size, message,
                buffer_size);
  }

  ~Message() { delete _data; }

  void moved() { _data = nullptr; }

  const char* sender() const { return _data->payload; }

  const char* receiver() const { return _data->payload + _data->sender_size; }

  uint32_t tag() const { return _data->tag; }

  const char* buffer() const {
    return _data->payload + _data->sender_size + _data->receiver_size;
  }

  size_t buffer_size() const { return _data->buffer_size; }

  size_t sender_size() const { return _data->sender_size; }

  size_t receiver_size() const { return _data->receiver_size; }

  size_t payload_size() const {
    return _data->receiver_size + _data->sender_size + _data->buffer_size;
  }

  void* raw() { return _data; }

 private:
  struct InternalDataStructure {
    constexpr static size_t size_overhead =
        3 * sizeof(size_t) + sizeof(uint32_t);

    size_t sender_size;
    size_t receiver_size;
    size_t buffer_size;
    uint32_t tag;
    char payload[0];
  };
  InternalDataStructure* _data;
};

#endif  // MAIN_INCLUDE_MESSAGE_HPP_
