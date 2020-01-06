idf_component_register(
  SRCS "main.cpp" "managed_actor.cpp" "board_functions.cpp" "router_v2_freertos.cpp" "routing.cpp" "publication.cpp"
  INCLUDE_DIRS ""
  REQUIRES "lua" "nvs_flash" "msgpack"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17 )