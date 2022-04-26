#include "publication.hpp"

#include "imports.hpp"

namespace uactor {

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
  Publication::Entry* ret = this->_data[key];
  if (ret == nullptr && id != 0) {
    ret = this->_data.create(key);
    ret->key = key;
    _pub_get_val_int(*reinterpret_cast<int*>(1), this->id, key, ret);

    // pub_get_val(this->id, key, ret);
  }
  return ret == nullptr || ret->_t == Type::VOID ? nullptr : ret;
}

void Publication::insert(const char* key, const Entry& entry) {
  this->_data.insert(key, entry);
}

// Publication::Entry* Publication::acc(const unsigned pos) {
//   return this->_data[pos];
// }

}  // namespace uactor
