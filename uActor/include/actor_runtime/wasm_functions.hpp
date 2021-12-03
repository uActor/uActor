#pragma once

#include <cstdint>

#include "pubsub/publication.hpp"
#include "wasm3.hpp"

namespace uActor::ActorRuntime {
template <typename T>
class Array {
 public:
  wasm3::pointer_t _data;
  wasm3::pointer_t size;
  Array(T data, wasm3::pointer_t size) : _data(data), size(size) {}
} __attribute__((packed));

class Publication {
 public:
  enum class Type : int32_t { STRING = 0, INT32 = 1, FLOAT = 3, BIN = 4 };
  struct Entry {
    Type _t;
    wasm3::pointer_t _elem;
    wasm3::pointer_t key;
  } __attribute__((packed));

  Array<Entry> _data;
} __attribute__((packed));

wasm3::pointer_t store_publication(wasm3::memory* m,
                                   const uActor::PubSub::Publication& pub);

void get_publication(wasm3::memory* m, const void* addr,
                     uActor::PubSub::Publication& ret);

wasm3::pointer_t uactor_malloc(int32_t id, wasm3::pointer_t size);

void uactor_free(int32_t id, wasm3::pointer_t addr);
void uactor_print(int32_t id, const void* string);

void uactor_publish(int32_t id, const void* addr);

}  // namespace uActor::ActorRuntime
