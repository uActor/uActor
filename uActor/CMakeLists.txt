file(GLOB_RECURSE SOURCES *.cpp)
file(GLOB_RECURSE HEADER *.hpp)

add_library(uActor STATIC ${SOURCES} ${HEADERS})
target_include_directories(uActor PUBLIC "include")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/lua ${CMAKE_CURRENT_BINARY_DIR}/lib_lua)
target_link_libraries(uActor lib_lua)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/msgpack ${CMAKE_CURRENT_BINARY_DIR}/lib_msgpack)
target_link_libraries(uActor lib_msgpack)

target_compile_options(uActor PRIVATE -std=c++17)