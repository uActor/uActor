#pragma once

#include "types.hpp"

namespace uactor {
class Publication;
class Subscription;
}  // namespace uactor

void* malloc(unsigned size);

template <typename T>
inline T* malloc(unsigned size) {
  return static_cast<T*>(malloc(sizeof(T) * size));
}

template <typename T>
inline T* malloc() {
  return static_cast<T*>(malloc(sizeof(T)));
}

void free(void* addr);

void memcpy(void* dest, const void* src, size_t size);

void print(const char*);
void publish(const uactor::Publication*);
void subscribe(const uactor::Subscription*);

void pub_get_val(size_t pub_id, const char* key, void* entry);

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
