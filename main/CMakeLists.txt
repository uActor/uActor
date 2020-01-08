idf_component_register(
  # TODO(raphaelhetzel) use SRC_DIRS once we can safely drop the broken legacy code
  SRCS "src_esp32/main.cpp" "src/managed_actor.cpp" "src_esp32/board_functions.cpp" "legacy/router_v2_freertos.cpp" "src_esp32/routing.cpp" "src/publication.cpp"
  INCLUDE_DIRS "include" "include_esp32"
  REQUIRES "lua" "nvs_flash" "msgpack"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17 )