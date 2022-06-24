#include "uactor.hpp"

#include "imports.hpp"
#include "utils.hpp"

void* malloc(unsigned size) {
  return reinterpret_cast<void*>(
      _malloc_int(*reinterpret_cast<pointer_t*>(1), size));
}

void memcpy(void* dest, const void* src, size_t size) {
  memcpy_int(dest, src, size);
}

void free(void* addr) {
  _free_int(*reinterpret_cast<int*>(1), reinterpret_cast<pointer_t>(addr));
}

void print(const char* string) {
  _print_int(*reinterpret_cast<int*>(1), static_cast<const void*>(string));
}

void publish(const uactor::Publication* pub) {
  _publish_int(*reinterpret_cast<int*>(1), static_cast<const void*>(pub));
}

void subscribe(const uactor::Subscription* sub) {
  _subscribe_int(*reinterpret_cast<int*>(1),
                 static_cast<const void*>(sub));
}

void pub_get_val(size_t pub_id, const char* key, void* entry) {
  _pub_get_val_int(*reinterpret_cast<int*>(1), pub_id, key, entry);
}
