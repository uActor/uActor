idf_component_register(
  SRCS "src/ble_actor.cpp"
  INCLUDE_DIRS "include"
  REQUIRES bt
)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)

if (NOT TARGET uActor)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)
endif()
target_link_libraries(${COMPONENT_LIB} uActor)

if (NOT TARGET lib_base64)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../vendor/base64 ${CMAKE_CURRENT_BINARY_DIR}/lib_base64)
endif()
target_link_libraries(${COMPONENT_LIB} lib_base64)