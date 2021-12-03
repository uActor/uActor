#pragma once

#ifdef WASM32
using int32_t = int;
using pointer_t = int;
using size_t = unsigned;
#else
static_assert(false, "Unsupported target");
#endif
