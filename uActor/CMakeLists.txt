cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE SOURCES *.cpp)
file(GLOB_RECURSE HEADER *.hpp)

if(NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 17)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

add_library(uActor STATIC ${SOURCES} ${HEADERS})
target_include_directories(uActor PUBLIC "include")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/lua ${CMAKE_CURRENT_BINARY_DIR}/lib_lua)
target_link_libraries(uActor lib_lua)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/msgpack ${CMAKE_CURRENT_BINARY_DIR}/lib_msgpack)
target_link_libraries(uActor lib_msgpack)

if (NOT TARGET lib_base64)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/base64 ${CMAKE_CURRENT_BINARY_DIR}/lib_base64)
endif()
target_link_libraries(uActor lib_base64)

if (NOT TARGET lib_blake2)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/blake2 ${CMAKE_CURRENT_BINARY_DIR}/lib_blake2)
endif()
target_link_libraries(uActor lib_blake2)

if (NOT TARGET frozen)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/frozen ${CMAKE_CURRENT_BINARY_DIR}/lib_frozen)
endif()
target_link_libraries(uActor frozen)
