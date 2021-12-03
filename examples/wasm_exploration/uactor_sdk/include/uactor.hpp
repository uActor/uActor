#pragma once

#include "types.hpp"

namespace uactor {
class Publication;
}

void* malloc(unsigned size);

template <typename T>
inline T* malloc(unsigned size) {
  return static_cast<T*>(malloc(size));
}

void free(void* addr);

void print(const char*);
void publish(const uactor::Publication*);

namespace uactor {
template <typename T>
class Array {
  T* _data;

 public:
  const unsigned size;

  Array(T* data, unsigned size) : _data(data), size(size) {}
  Array(unsigned size)
      : _data(reinterpret_cast<T*>(malloc(size * sizeof(T)))), size(size) {}

  [[nodiscard]] T* operator[](const unsigned pos) { return &_data[pos]; }
} __attribute__((packed));

// class String {
//   char* _str;

//  public:
//   String(char* str) : _str(str) {}
//   const char* c_str();
//   String(String& other) = default;
//   String(String&&) = default;

//   [[nodiscard]] size_t size() const;
//   String& operator=(const String& other) = default;
//   [[nodiscard]] bool operator==(const String& other) const;
//   [[nodiscard]] bool operator==(const char* other) const;
// } __attribute__((packed));

class Publication {
 public:
  enum class Type : int32_t { STRING = 0, INT32 = 1, FLOAT = 3, BIN = 4 };
  struct Entry {
    Type _t;
    const void* _elem;
    const char* key;
  } __attribute__((packed));

 private:
  Array<Entry> _data;

 public:
  explicit Publication(unsigned size);
  [[nodiscard]] unsigned size() const;
  // Entry* find(const String& key);
  Entry* find(const char* key);
  // Entry* operator[](const String& key);
  Entry* operator[](const char* key);
  Entry* acc(const unsigned pos);
} __attribute__((packed));

}  // namespace uactor
