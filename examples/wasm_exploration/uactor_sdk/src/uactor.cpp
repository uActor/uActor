#include "uactor.hpp"

#include "imports.hpp"
#include "utils.hpp"

void* malloc(unsigned size) {
  return reinterpret_cast<void*>(
      _malloc_int(*reinterpret_cast<pointer_t*>(1), size));
}

void free(void* addr) {
  _free_int(*reinterpret_cast<int*>(1), reinterpret_cast<pointer_t>(addr));
}

void print(const char* string) {
  _print_int(*reinterpret_cast<int*>(1), reinterpret_cast<const void*>(string));
}

void publish(const uactor::Publication* pub) {
  _publish_int(*reinterpret_cast<int*>(1), reinterpret_cast<const void*>(pub));
}

namespace uactor {

// size_t String::size() const { return 0; }

// const char* String::c_str() { return this->_str; }

// bool String::operator==(const String& other) const {
//   int i = 0;
//   for (; this->_str[i] != '\0' && other._str[i] != '\0'; i++) {
//     if (this->_str[i] != other._str[i]) {
//       return false;
//     }
//   }
//   return this->_str[i] == other._str[i];
// }

// bool String::operator==(const char* other) const {
//   int i = 0;
//   for (; this->_str[i] != '\0' && other[i] != '\0'; i++) {
//     if (this->_str[i] != other[i]) {
//       return false;
//     }
//   }
//   return this->_str[i] == other[i];
// }

Publication::Publication(unsigned size) : _data(size) {}

unsigned Publication::size() const { return this->_data.size; }

// Publication::Entry* Publication::find(const String& key) {
//   return this->operator[](key);
// }

Publication::Entry* Publication::find(const char* key) {
  return this->operator[](key);
}

// Publication::Entry* Publication::operator[](const String& key) {
//   for (unsigned i = 0; i < this->size(); i++) {
//     if (key == this->_data[i]->key) {
//       return this->_data[i];
//     }
//   }
//   return nullptr;
// }

Publication::Entry* Publication::operator[](const char* key) {
  for (unsigned i = 0; i < this->size(); i++) {
    if (string_compare(this->_data[i]->key, key)) {
      return this->_data[i];
    }
  }
  return nullptr;
}

Publication::Entry* Publication::acc(const unsigned pos) {
  return this->_data[pos];
}

}  // namespace uactor
