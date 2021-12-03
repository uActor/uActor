#include "actor_runtime/wasm_functions.hpp"

#include "actor_runtime/managed_wasm_actor.hpp"
#include "support/logger.hpp"

namespace uActor::ActorRuntime {

wasm3::pointer_t store_publication(wasm3::memory* m,
                                   const uActor::PubSub::Publication& pub) {
  wasm3::pointer_t ret = m->malloc(sizeof(Publication));
  if (ret == 0) {
    return 0;
  }

  Publication* wasm_pub = reinterpret_cast<Publication*>(m->access(ret));
  wasm_pub->_data._data = m->malloc(sizeof(Publication::Entry) * pub.size());
  wasm_pub->_data.size = pub.size();
  wasm3::pointer_t i = 0;
  for (const auto& pub_it : pub) {
    Publication::Entry* cur_entry = reinterpret_cast<Publication::Entry*>(
        m->access(wasm_pub->_data._data + sizeof(Publication::Entry) * i++));
    cur_entry->key = m->store_str(pub_it.first);
    if (std::holds_alternative<uActor::PubSub::Publication::AString>(
            pub_it.second)) {
      cur_entry->_t = Publication::Type::STRING;
      cur_entry->_elem = m->store_str(
          std::get<uActor::PubSub::Publication::AString>(pub_it.second));

    } else if (std::holds_alternative<int32_t>(pub_it.second)) {
      cur_entry->_t = Publication::Type::INT32;
      cur_entry->_elem = m->store(std::get<int32_t>(pub_it.second));
    } else if (std::holds_alternative<float>(pub_it.second)) {
      cur_entry->_t = Publication::Type::FLOAT;
      cur_entry->_elem = m->store(std::get<float>(pub_it.second));
    } else if (std::holds_alternative<uActor::PubSub::Publication::bin_type>(
                   pub_it.second)) {
      cur_entry->_t = Publication::Type::BIN;
      cur_entry->_elem = m->malloc(sizeof(Array<uint8_t>));
      // todo store bin type
    } else {
      uActor::Support::Logger::error("WASM ACTOR",
                                     "Error unhandled publication type");
      assert(false);
    }
  }
  return ret;
}

void get_publication(wasm3::memory* m, const void* addr,
                     uActor::PubSub::Publication& ret) {
  const Publication* pub = reinterpret_cast<const Publication*>(addr);
  const Publication::Entry* entry_arr =
      reinterpret_cast<Publication::Entry*>(m->access(pub->_data._data));
  for (size_t i = 0; i < pub->_data.size; i++) {
    switch (entry_arr[i]._t) {
      case Publication::Type::STRING:
        ret.set_attr(
            reinterpret_cast<const char*>(m->access(entry_arr[i].key)),
            reinterpret_cast<const char*>(m->access(entry_arr[i]._elem)));
        break;
      case Publication::Type::FLOAT:
        ret.set_attr(reinterpret_cast<const char*>(m->access(entry_arr[i].key)),
                     *reinterpret_cast<float*>(m->access(entry_arr[i]._elem)));
        break;
      case Publication::Type::INT32:
        ret.set_attr(
            reinterpret_cast<const char*>(m->access(entry_arr[i].key)),
            *reinterpret_cast<int32_t*>(m->access(entry_arr[i]._elem)));
        break;
      case Publication::Type::BIN:
        // todo add me runs to default for now
      default:
        uActor::Support::Logger::error("WASM ACTOR",
                                       "Error unhandled publication type");
        assert(false);
    }
  }
}

wasm3::pointer_t uactor_malloc(int32_t id, wasm3::pointer_t size) {
  return static_cast<int32_t>(wasm_memory::get_wasm_mem_storage()[id]->malloc(
      static_cast<uint32_t>(size)));
}

void uactor_free(int32_t id, wasm3::pointer_t addr) {
  wasm_memory::get_wasm_mem_storage()[id]->free(addr);
}

void uactor_print(int32_t id, const void* string) {
  if (string == nullptr) {
    return;
  }
  std::cout << reinterpret_cast<const char*>(string) << std::endl;
}

void uactor_publish(int32_t id, const void* addr) {
  if (addr == nullptr) {
    return;
  }
  uActor::PubSub::Publication pub;
  get_publication(wasm_memory::get_wasm_mem_storage()[id], addr, pub);

  const ManagedWasmActor* wasm_act = reinterpret_cast<const ManagedWasmActor*>(
      wasm_memory::get_wasm_mem_storage().access_managed_wasm_actor(id));

  wasm_act->ext_publish(std::move(pub));
}
}  // namespace uActor::ActorRuntime
