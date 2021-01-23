idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES "sensiron-scd" "nvs_flash"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)

if (NOT TARGET uActor)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)
endif()
target_link_libraries(${COMPONENT_LIB} uActor)