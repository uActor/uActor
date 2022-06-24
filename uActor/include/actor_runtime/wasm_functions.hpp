#pragma once

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pubsub/publication.hpp"
#include "wasm3.hpp"

namespace uActor::ActorRuntime {

struct PublicationStorage {
  struct PubVecEntry {
    bool free = false;
    uActor::PubSub::Publication pub;
    explicit PubVecEntry(uActor::PubSub::Publication&& pub)
        : pub(std::move(pub)) {}
  };
  std::mutex mtx;
  std::vector<PubVecEntry> pub_vec{PubVecEntry{{}}};

  size_t insert(uActor::PubSub::Publication&& pub);
  void erase(size_t id);
  PubSub::Publication& operator[](size_t id);
};

PublicationStorage& get_publication_storage();

namespace WasmDataStructures {
template <typename KeyType, wasm3::size_t TableSize>
struct uactor_hash_func {
  static constexpr wasm3::size_t hash(const KeyType& key) = delete;
};

template <wasm3::size_t TableSize>
struct uactor_hash_func<const char*, TableSize> {
  static constexpr wasm3::size_t hash(const char* key) {
    wasm3::size_t ret = 0;
    for (wasm3::size_t i = 0; key[i] != '\0'; i++) {
      ret += key[i];
      ret = ret % TableSize;
    }
    return ret;
  }
};

template <typename T>
struct Array {
 public:
  wasm3::pointer_t _data = 0;
  wasm3::size_t size;
  static wasm3::pointer_t create(wasm3::size_t size, wasm3::memory* m) {
    wasm3::pointer_t data = m->malloc(sizeof(T) * size);
    if (data == 0) {
      return 0;
    }
    wasm3::pointer_t ret = m->malloc(sizeof(Array<T>));
    if (ret == 0) {
      m->free(data);
      return 0;
    }
    Array<T>* arr = static_cast<Array<T>*>(m->access(ret));
    arr->_data = data;
    arr->size = size;
    return ret;
  }

  void init(wasm3::size_t size, wasm3::memory* m) {
    this->_data = m->malloc(sizeof(T) * size);
    if (this->_data == 0) {
      this->size = 0;
    } else {
      this->size = size;
    }
  }

  void free(wasm3::memory* m) { m->free(this->_data); }

  static void free(wasm3::pointer_t _array, wasm3::memory* m) {
    assert(_array != 0);
    Array<T>* array = static_cast<Array<T>*>(m->access(_array));
    array->free(m);
    m->free(_array);
  }
  T* access(wasm3::memory* m, size_t i) {
    assert(i < size);
    return static_cast<T*>(m->access(_data + (sizeof(T) * i)));
  }
  wasm3::pointer_t _access(wasm3::memory* m, size_t i) {
    assert(i < size);
    return _data + sizeof(T) * i;
  }
} __attribute__((packed));

template <typename T>
class List {
 public:
  struct ListEntry {
    wasm3::pointer_t _next, _prev;
    T val;
  } __attribute__((packed));
  wasm3::pointer_t _list_head = 0;

  static wasm3::pointer_t create(wasm3::memory* m) {
    wasm3::pointer_t _list = m->malloc(sizeof(List));
    List<T>* list = static_cast<List<T>*>(m->access(_list));
    list->init();
    return _list;
  }

  void init() { this->_list_head = 0; }

  void free(wasm3::memory* m) {
    wasm3::pointer_t _head = this->_list_head;
    while (_head != 0) {
      ListEntry* head = static_cast<ListEntry*>(m->access(_head));
      wasm3::pointer_t _nx = head->_next;
      m->free(_head);
      _head = _nx;
    }
  }

  static void free(wasm3::pointer_t _list, wasm3::memory* m) {
    assert(_list != 0);
    List<T>* list = static_cast<List<T>*>(m->access(_list));
    list->free(m);
    m->free(_list);
  }

  wasm3::pointer_t reserve(wasm3::memory* m) {
    wasm3::pointer_t _entry = m->malloc(sizeof(ListEntry));
    ListEntry* entry = static_cast<ListEntry*>(m->access(_entry));
    entry->_next = this->_list_head;
    entry->_prev = 0;
    if (this->_list_head != 0) {
      ListEntry* list_head =
          static_cast<ListEntry*>(m->access(this->_list_head));
      list_head->_prev = _entry;
    }
    this->_list_head = _entry;
    return _entry;
  }

  wasm3::pointer_t append(T* val, wasm3::memory* m) {
    assert(val != nullptr);
    wasm3::pointer_t _ret = this->reserve();
    ListEntry* elem = static_cast<ListEntry*>(m->access(_ret));
    memcpy(&elem->val, val, sizeof(T));
    return _ret;
  }
} __attribute__((packed));

template <typename ValType, wasm3::size_t TableSize = 13,
          typename HashFunc = uactor_hash_func<const char*, TableSize>>
class StringHashMap {
 public:
  struct Bucket {
    wasm3::pointer_t _key = 0;
    wasm3::pointer_t _val = 0;
    wasm3::pointer_t _next_bucket = 0;
    static wasm3::pointer_t create(wasm3::memory* m) {
      wasm3::pointer_t _bucket = m->malloc(sizeof(Bucket));
      if (_bucket == 0) {
        return 0;
      }
      Bucket* bucket = static_cast<Bucket*>(m->access(_bucket));
      bucket->_key = 0;
      bucket->_val = 0;
      bucket->_next_bucket = 0;
    }
  } __attribute__((packed));

  List<ValType> data{};
  Array<wasm3::pointer_t> map_arr{};

  void init(wasm3::memory* m) {
    this->data.init();
    this->map_arr.init(TableSize, m);
    for (wasm3::size_t i = 0; i < TableSize; i++) {
      *this->map_arr.access(m, i) = 0;
    }
  }
  // todo rename
  static wasm3::pointer_t create(wasm3::memory* m) {
    wasm3::pointer_t _map =
        m->malloc(sizeof(StringHashMap<ValType, TableSize, HashFunc>));
    StringHashMap* map = static_cast<StringHashMap*>(m->access(_map));
    map->init(m);
    return _map;
  }

  void free(wasm3::memory* m) {
    for (wasm3::size_t i = 0; i < TableSize; i++) {
      wasm3::pointer_t _cur_bucket = this->map_arr.access(m, i);
      while (_cur_bucket != 0) {
        Bucket* cur_bucket = static_cast<Bucket*>(m->access(_cur_bucket));
        wasm3::pointer_t _nx = cur_bucket->_next_bucket;
        m->free(_cur_bucket);
        _cur_bucket = _nx;
      }
      this->data.free(m);
      this->map_arr.free(m);
    }
  }

  static void free(wasm3::pointer_t _map, wasm3::memory* m) {
    StringHashMap* map = static_cast<StringHashMap*>(m->access(_map));
    map->free(m);
    m->free(_map);
  }
} __attribute__((packed));

}  // namespace WasmDataStructures

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
    wasm3::pointer_t _elem;
    wasm3::pointer_t _key;
  } __attribute__((packed));
  wasm3::size_t id;
  WasmDataStructures::StringHashMap<Entry> _data;
} __attribute__((packed));

wasm3::pointer_t store_publication(wasm3::memory* m,
                                   const uActor::PubSub::Publication& pub);
// todo Raphael, should we allow non const pointers or should this be a Pointer?
void get_publication(wasm3::memory* m, const void* addr,
                      uActor::PubSub::Publication& ret);

wasm3::pointer_t uactor_malloc(int32_t id, wasm3::pointer_t size);

void uactor_memcpy(void* dst, const void* src, wasm3::size_t size);
void uactor_free(int32_t id, wasm3::pointer_t addr);
void uactor_print(int32_t id, const void* string);

void uactor_publish(int32_t id, const void* addr);

void uactor_subscribe(int32_t id, const void* addr);

void uactor_get_val(int32_t mem_id, int32_t pub_id, const void* _key,
                    void* pub_entry);
}  // namespace uActor::ActorRuntime
