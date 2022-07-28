
#include "actor_runtime/wasm_functions.hpp"

#include <cstring>
#include <string_view>
#include <vector>

#include "actor_runtime/managed_wasm_actor.hpp"
#include "pubsub/constraint.hpp"
#include "pubsub/filter.hpp"
#include "pubsub/publication.hpp"
#include "support/logger.hpp"
#include "wasm3.hpp"

namespace uActor::ActorRuntime {

size_t WASMPublicationStorage::insert(uActor::PubSub::Publication&& pub) {
  mtx.lock();
  size_t pos;
  auto search = std::find_if(pub_vec.begin(), pub_vec.end(),
                             [](const auto& c) { return c.free; });
  if (search != pub_vec.end()) {
    search->pub = pub;
    search->free = false;
    pos = std::distance(pub_vec.begin(), search);
  } else {
    pos = pub_vec.size();
    pub_vec.emplace_back(std::move(pub));
  }
  mtx.unlock();
  return pos;
}

void WASMPublicationStorage::erase(size_t id) {
  mtx.lock();
  assert(pub_vec.size() > id);
  pub_vec[id].free = true;
  mtx.unlock();
}

PubSub::Publication& WASMPublicationStorage::operator[](size_t id) {
  assert(pub_vec.size() > id);
  assert(!pub_vec[id].free);
  return pub_vec[id].pub;
}

WASMPublicationStorage& get_publication_storage() {
  static WASMPublicationStorage pubstore{};
  return pubstore;
}

void get_publication(wasm3::memory* m, const void* addr,
                     uActor::PubSub::Publication* ret) {
  const WASMPublication* pub = static_cast<const WASMPublication*>(addr);
  // Publication is based on previous publication --> we must merge
  if (pub->id != 0) {
    auto& pub_store = get_publication_storage();
    *ret = std::move(pub_store[pub->id]);
    pub_store.erase(pub->id);
  }
  wasm3::pointer_t _cur_entry = pub->_data.data._list_head;
  while (_cur_entry != 0) {
    WasmDataStructures::List<WASMPublication::Entry>::ListEntry* cur_entry =
        static_cast<
            WasmDataStructures::List<WASMPublication::Entry>::ListEntry*>(
            m->access(_cur_entry));
    switch (cur_entry->val._t) {
      case WASMPublication::Type::STRING:
        ret->set_attr(static_cast<const char*>(m->access(cur_entry->val._key)),
                     static_cast<const char*>(m->access(cur_entry->val._elem)));
        break;
      case WASMPublication::Type::FLOAT:
        ret->set_attr(static_cast<const char*>(m->access(cur_entry->val._key)),
                     *static_cast<float*>(m->access(cur_entry->val._elem)));
        break;
      case WASMPublication::Type::INT32:
        ret->set_attr(static_cast<const char*>(m->access(cur_entry->val._key)),
                     *static_cast<int32_t*>(m->access(cur_entry->val._elem)));
        break;
      case WASMPublication::Type::VOID:
        ret->erase_attr(
            static_cast<const char*>(m->access(cur_entry->val._key)));
      case WASMPublication::Type::BIN:
        // Bin Type is currently not supported, will run through to default
      default:
        uActor::Support::Logger::error("WASM ACTOR",
                                       "Error unhandled publication type");
        assert(false);
    }
    _cur_entry = cur_entry->_next;
  }
}

wasm3::pointer_t uactor_malloc(int32_t id, wasm3::pointer_t size) {
  return static_cast<int32_t>(wasm_memory::get_wasm_mem_storage()[id]->malloc(
      static_cast<uint32_t>(size)));
}

void uactor_memcpy(void* dst, const void* src, wasm3::size_t size) {
  std::memcpy(dst, src, size);
}

void uactor_free(int32_t id, wasm3::pointer_t addr) {
  wasm_memory::get_wasm_mem_storage()[id]->free(addr);
}

void uactor_print(int32_t id, const void* string) {
  if (string == nullptr) {
    return;
  }
  Support::Logger::log("WASM_ACTOR",
                       "Printed:", static_cast<const char*>(string));
}

void uactor_publish(int32_t id, const void* addr) {
  if (addr == nullptr) {
    return;
  }
  uActor::PubSub::Publication pub;
  get_publication(wasm_memory::get_wasm_mem_storage()[id], addr, &pub);

  ManagedWasmActor* wasm_act = static_cast<ManagedWasmActor*>(
      wasm_memory::get_wasm_mem_storage().access_managed_wasm_actor(id));

  wasm_act->ext_publish(std::move(pub));
}

void uactor_subscribe(int32_t id, const void* addr) {
  if (addr == nullptr) {
    return;
  }

  uActor::PubSub::Publication pub;
  get_publication(wasm_memory::get_wasm_mem_storage()[id], addr, &pub);

  std::vector<PubSub::Constraint> vec;

  for (const auto& val : pub) {
    if (std::holds_alternative<PubSub::Publication::AString>(val.second)) {
      vec.emplace_back(std::string(val.first),
                       std::get<PubSub::Publication::AString>(val.second));

    } else if (std::holds_alternative<int32_t>(val.second)) {
      vec.emplace_back(std::string(val.first), std::get<int32_t>(val.second));
    } else if (std::holds_alternative<float>(val.second)) {
      vec.emplace_back(std::string(val.first), std::get<float>(val.second));
    }
  }

  PubSub::Filter f(vec);

  ManagedWasmActor* wasm_act = static_cast<ManagedWasmActor*>(
      wasm_memory::get_wasm_mem_storage().access_managed_wasm_actor(id));
  wasm_act->ext_subscribe(std::move(f));
}

void uactor_get_val(int32_t mem_id, int32_t pub_id, const void* _key,
                    void* pub_entry) {
  wasm3::memory* m = wasm_memory::get_wasm_mem_storage()[mem_id];
  uActor::PubSub::Publication& pub = get_publication_storage()[pub_id];
  WASMPublication::Entry* cur_entry =
      static_cast<WASMPublication::Entry*>(pub_entry);
  const char* key = static_cast<const char*>(_key);
  if (!pub.has_attr(key)) {
    cur_entry->_t = WASMPublication::Type::VOID;
    cur_entry->_elem = 0;
    return;
  }

  const auto pub_entry_handle = pub.get_attr(static_cast<const char*>(key));
  if (std::holds_alternative<std::string_view>(pub_entry_handle)) {
    cur_entry->_t = WASMPublication::Type::STRING;
    cur_entry->_elem =
        m->store_str(std::string(std::get<std::string_view>(pub_entry_handle)));

  } else if (std::holds_alternative<int32_t>(pub_entry_handle)) {
    cur_entry->_t = WASMPublication::Type::INT32;
    cur_entry->_elem = m->store(std::get<int32_t>(pub_entry_handle));
  } else if (std::holds_alternative<float>(pub_entry_handle)) {
    cur_entry->_t = WASMPublication::Type::FLOAT;
    cur_entry->_elem = m->store(std::get<float>(pub_entry_handle));
  } else {
    uActor::Support::Logger::error("WASM ACTOR",
                                   "Error unhandled publication type");
    assert(false);
  }
}

}  // namespace uActor::ActorRuntime
