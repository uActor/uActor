#pragma once

#include "types.hpp"

namespace uactor {
class Publication;
class Subscription;
namespace Publicaton {
struct Entry;
} // namespace Publicaton
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
