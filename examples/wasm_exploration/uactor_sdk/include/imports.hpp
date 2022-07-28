#pragma once

#include "types.hpp"
#include "uactor.hpp"

#define WASM_IMPORT(module, name) \
  __attribute__((import_module(module))) __attribute__((import_name(name)))

#define WASM_EXPORT extern "C" __attribute__((visibility("default")))
// These functions are not meant to be called by the user,
// only used by the sdk to communicate with the runtime
WASM_IMPORT("_uactor", "malloc")
pointer_t _malloc_int(pointer_t, int32_t);
WASM_IMPORT("_uactor", "memcpy")
void memcpy_int(void* dst, const void* src, size_t size);
WASM_IMPORT("_uactor", "free")
void _free_int(int32_t, pointer_t);
WASM_IMPORT("_uactor", "print")
void _print_int(int32_t, const void* string);
WASM_IMPORT("_uactor", "publish")
void _publish_int(int32_t, const void* publication);
WASM_IMPORT("_uactor", "subscribe")
void _subscribe_int(int32_t, const void* subscription);
WASM_IMPORT("_uactor", "get_val")
void _pub_get_val_int(int32_t, int32_t pub_id, const void* key,
                      void* pub_entry);
