#pragma once

#include "data_structures.hpp"

namespace uactor {
class Publication {
 public:
  enum class Type : int32_t {
    STRING = 0,
    INT32 = 1,
    FLOAT = 3,
    BIN = 4,
    VOID = 5
  };
  struct Entry {
    Type _t;
    const void* _elem;
    const char* key;
    Entry(Type _t, const char* key, const void* _elem)
        : _t(_t), _elem(_elem), key(key) {}
  } __attribute__((packed));

 private:
  const size_t id = 0;
  StringHashMap<Entry> _data{};

 public:
  Publication() = default;
  explicit Publication(size_t id) : id(id) {}
  ~Publication() = default;

  Entry* find(const char* key);

  Entry* operator[](const char* key);

  void insert(const char* key, const Entry& entry);
} __attribute__((packed));
}  // namespace uactor
