#ifndef MAIN_MESSAGE_HPP_
#define MAIN_MESSAGE_HPP_

#include <cassert>
#include <cstdint>
#include <cstring>

#include "benchmark_configuration.hpp"

struct Tags {
  enum WELL_KNOWN_TAGS : uint32_t {
    INIT = 1,
    SPAWN_LUA_ACTOR,
    TIMEOUT,
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
      case 4:  // every value requested should be defined, hence no default
        return nullptr;
    }
    return nullptr;
  }
};

class Message {
 public:
  Message(const Message& old) = delete;
  Message(Message&& old) : _data(old._data) { old._data = nullptr; }

  Message& operator=(Message&& old) {
    if (_data) {
      delete reinterpret_cast<char*>(_data);
    }
    _data = old._data;
    old._data = nullptr;
    return *this;
  }

  explicit Message(size_t size, bool init = true) {
    if (init) {
      _data = reinterpret_cast<InternalDataStructure*>(
          new char[size + InternalDataStructure::size_overhead]);
      // assert(_data);
      _data->size = size;
    } else {
      _data = nullptr;
    }
  }

  Message(uint64_t sender, uint64_t receiver, uint32_t tag,
          const char* message) {
    size_t buffer_size = strlen(message) + 1;
    _data = reinterpret_cast<InternalDataStructure*>(
        new char[buffer_size + InternalDataStructure::size_overhead]);
    _data->sender = sender;
    _data->receiver = receiver;
    _data->size = buffer_size;
    _data->tag = tag;

    std::strncpy(buffer(), message, buffer_size);
  }

  ~Message() { delete _data; }

  void moved() { _data = nullptr; }

  uint64_t sender() const { return _data->sender; }

  uint64_t receiver() const { return _data->receiver; }

  size_t size() const { return _data->size; }

  uint32_t tag() const { return _data->tag; }

  const char* ro_buffer() const {
    return static_cast<const char*>(_data->payload);
  }

  void sender(uint64_t sender) { _data->sender = sender; }

  void receiver(uint64_t receiver) { _data->receiver = receiver; }

  void tag(uint32_t tag) { _data->tag = tag; }

  char* buffer() { return _data->payload; }

  void* raw() { return _data; }

 private:
  struct InternalDataStructure {
    constexpr static size_t size_overhead =
        2 * sizeof(uint64_t) + 2 * sizeof(size_t);

    uint64_t sender;
    uint64_t receiver;
    size_t size;
    size_t tag;
    char payload[0];
  };
  InternalDataStructure* _data;
};

#endif  //  MAIN_MESSAGE_HPP_
