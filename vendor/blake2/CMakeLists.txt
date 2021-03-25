add_library(lib_blake2 STATIC blake2_impl/ref/blake2s-ref.c)
target_include_directories(lib_blake2 PUBLIC "blake2_impl/ref/")