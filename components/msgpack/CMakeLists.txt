# Header only library
idf_component_register(
  INCLUDE_DIRS "msgpack-c/include/"
)
target_compile_definitions(${COMPONENT_LIB} INTERFACE MSGPACK_ZONE_CHUNK_SIZE=128)