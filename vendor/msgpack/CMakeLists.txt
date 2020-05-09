file(GLOB_RECURSE HEADER msgpack-c/include/*)

add_library(lib_msgpack INTERFACE)
target_include_directories(lib_msgpack INTERFACE "msgpack-c/include/")
target_compile_definitions(lib_msgpack INTERFACE MSGPACK_ZONE_CHUNK_SIZE=128)