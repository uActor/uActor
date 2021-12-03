#pragma once

#include "types.hpp"
#include "uactor.hpp"

#define WASM_IMPORT(module, name) \
  __attribute__((import_module(module))) __attribute__((import_name(name)))

#define WASM_EXPORT extern "C" __attribute__((visibility("default")))

WASM_IMPORT("_uactor", "malloc")
pointer_t _malloc_int(pointer_t, int32_t);
WASM_IMPORT("_uactor", "free")
void _free_int(int32_t, pointer_t);
WASM_IMPORT("_uactor", "print")
void _print_int(int32_t, const void* string);
WASM_IMPORT("_uactor", "publish")
void _publish_int(int32_t, const void* publication);
