idf_component_register(
  # TODO(raphaelhetzel) use SRC_DIRS once we can safely drop the broken legacy code
  SRC_DIRS  "src/actor_runtime" "src/controllers" "src/pubsub" "src/remote" "src_esp32" "src_esp32/pubsub" "src_esp32/remote"
  INCLUDE_DIRS "include" "include_esp32"
  REQUIRES "lua" "nvs_flash" "msgpack" "wpa_supplicant" "testbed" "bmp180"
  EMBED_TXTFILES ./binary/eduroam.pem
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)
target_compile_options(${COMPONENT_LIB} PUBLIC -DESP_IDF)