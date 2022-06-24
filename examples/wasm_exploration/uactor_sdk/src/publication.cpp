#include "publication.hpp"

#include "uactor.hpp"

namespace uactor {


Publication::Entry* Publication::find(const char* key) {
  return this->operator[](key);
}

Publication::Entry* Publication::operator[](const char* key) {
  Publication::Entry* ret = this->_data[key];
  if (ret == nullptr && id != 0) {
    ret = this->_data.create(key);
    ret->key = key;

    pub_get_val(this->id, key, static_cast<void*>(ret));
  }
  return ret == nullptr || ret->_t == Type::VOID ? nullptr : ret;
}

void Publication::insert(const char* key, const Entry& entry) {
  this->_data.insert(key, entry);
}

}  // namespace uactor
