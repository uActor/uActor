idf_component_register(
  # TODO(raphaelhetzel) use SRC_DIRS once we can safely drop the broken legacy code
  SRC_DIRS  "src_esp32" "src_esp32/pubsub" "src_esp32/remote"
  INCLUDE_DIRS "include_esp32"
  REQUIRES "nvs_flash" "wpa_supplicant" "bmp180_actor" "epaper_actor" "ble_actor" "scd30_actor"
  EMBED_TXTFILES ./binary/eduroam.pem
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)

if (NOT TARGET uActor)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)
endif()

target_link_libraries(${COMPONENT_LIB} uActor)
target_compile_options(uActor PUBLIC -DESP_IDF)