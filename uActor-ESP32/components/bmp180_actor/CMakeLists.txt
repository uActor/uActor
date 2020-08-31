idf_component_register(
  SRCS "bmp180.c"
  INCLUDE_DIRS "."
)

if (NOT TARGET uActor)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)
endif()
target_link_libraries(${COMPONENT_LIB} uActor)