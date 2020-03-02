idf_component_register(
  # TODO(raphaelhetzel) use SRC_DIRS once we can safely drop the broken legacy code
  SRCS "src_esp32/main.cpp" "src/managed_actor.cpp" "src_esp32/board_functions.cpp" "legacy/router_v2_freertos.cpp" "src_esp32/pubsub/receiver.cpp" "src/pubsub/publication.cpp" "src/native_actor.cpp" "src/remote_connection.cpp" "src/pubsub/router.cpp" "src/pubsub/filter.cpp" "src/pubsub/constraint.cpp" "src/pubsub/constraint_index.cpp" "src/pubsub/subscription.cpp"
  INCLUDE_DIRS "include" "include_esp32"
  REQUIRES "lua" "nvs_flash" "msgpack" "wpa_supplicant" "testbed" "bmp180"
  EMBED_TXTFILES ./binary/eduroam.pem
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)