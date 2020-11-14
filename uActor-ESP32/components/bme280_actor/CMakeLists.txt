idf_component_register(
  SRCS "BME280_driver/bme280.c" "src/bme280_actor.cpp"
  INCLUDE_DIRS "BME280_driver" "include"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)

if (NOT TARGET uActor)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)
endif()
target_link_libraries(${COMPONENT_LIB} uActor)