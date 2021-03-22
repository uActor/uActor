add_library(lib_blake2 STATIC BLAKE2/ref/blake2s-ref.c)
target_include_directories(lib_blake2 PUBLIC "BLAKE2/ref/")