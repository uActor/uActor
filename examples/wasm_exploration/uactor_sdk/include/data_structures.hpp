#pragma once

#include "uactor.hpp"
#include "types.hpp"
#include "utils.hpp"

template <typename KeyType, size_t TableSize>
struct uactor_hash_func {
  // generic hashing is not implemented yet
  static constexpr size_t hash(const KeyType& key) = delete;
};

template <size_t TableSize>
struct uactor_hash_func<const char*, TableSize> {
  static constexpr size_t hash(const char*& key) {
    size_t ret = 0;
    for (size_t i = 0; key[i] != '\0'; i++) {
      ret += key[i];
      ret = ret % TableSize;
    }
    return ret;
  }
};

template <typename T>
class Array {
  T* _data = nullptr;

 public:
  const size_t size = 0;

  // Array(T* data, size_t size) : _data(data), size(size) {}
  Array(size_t size) : _data(malloc(size * sizeof(T))), size(size) {}
  Array(size_t size, T value) : _data(malloc<T>(size * sizeof(T))), size(size) {
    for (size_t i = 0; i < size; i++) {
      this->_data[i] = value;
    }
  }
  ~Array() { free(_data); }

  [[nodiscard]] T* operator[](const size_t pos) {
    return _data + (sizeof(T) * pos);
  }
} __attribute__((packed));

template <typename T>
class List {
 public:
  struct ListEntry {
    ListEntry *next = nullptr, *prev = nullptr;
    T val;
  } __attribute__((packed));

 private:
  ListEntry* head = nullptr;

 public:
  List() = default;
  ~List() {
    ListEntry* cur = head;
    while (cur != nullptr) {
      ListEntry* nx = cur->next;
      free(cur);
      cur = nx;
    }
  }
  ListEntry* reserve() {
    ListEntry* entry = malloc<ListEntry>(sizeof(ListEntry));
    entry->next = this->head;
    entry->prev = nullptr;
    if (this->head != nullptr) {
      this->head->prev = entry;
    }
    this->head = entry;
    return entry;
  }

  T* append(T* val) {
    T* ret = this->reserve();
    memcpy(&ret->val, val, sizeof(T));
    return ret;
  }

} __attribute__((packed));

template <typename ValType, size_t TableSize = 13,
          typename HashFunc = uactor_hash_func<const char*, TableSize>>
class StringHashMap {
  struct Bucket {
    const char* key;
    typename List<ValType>::ListEntry* val = nullptr;
    Bucket* next = nullptr;
  } __attribute__((packed));

  List<ValType> data{};
  Array<Bucket*> map_arr{TableSize, nullptr};

 public:
  StringHashMap() = default;
  ~StringHashMap() {
    for (size_t i = 0; i < TableSize; i++) {
      Bucket* cur = *map_arr[i];
      while (cur != nullptr) {
        Bucket* nx = cur->next;
        free(cur);
        cur = nx;
      }
    }
  }

  ValType* create(const char* key) {
    typename List<ValType>::ListEntry* val = data.reserve();
    Bucket** b = map_arr[HashFunc::hash(key)];
    bool existing_key = false;
    while (*b != nullptr) {
      if (string_compare(key, (*b)->key)) {
        existing_key = true;
        break;
      }
      b = &((*b)->next);
    }
    if (not existing_key) {
      *b = malloc<Bucket>(sizeof(Bucket));
      (*b)->key = key;
      (*b)->next = nullptr;
    } else {
      if ((*b)->val != nullptr) {
        free((*b)->val);
      }
    }
    (*b)->val = val;
    return &val->val;
  }

  void insert(const char* key, const ValType& val) {
    ValType* ret = this->create(key);
    memcpy(ret, &val, sizeof(ValType));
  }

  [[nodiscard]] ValType* find(const char* key) {
    auto a = HashFunc::hash(key);
    Bucket** b = map_arr[a];
    while ((*b) != nullptr) {
      if (string_compare(key, (*b)->key)) {
        return &((*b)->val->val);
      }
      b = &((*b)->next);
    }
    return nullptr;
  }

  [[nodiscard]] ValType* operator[](const char* key) { return this->find(key); }
} __attribute__((packed));
