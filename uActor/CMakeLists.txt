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

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/wasm3_cpp/ ${CMAKE_CURRENT_BINARY_DIR}/wasm3_cpp)
target_link_libraries(uActor wasm3_cpp)

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

if (CONFIG_UACTOR_V8)
target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_V8=1)
if (NOT TARGET lib_v8)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/v8 ${CMAKE_CURRENT_BINARY_DIR}/lib_v8)
endif()
target_link_libraries(uActor lib_v8)
endif()
