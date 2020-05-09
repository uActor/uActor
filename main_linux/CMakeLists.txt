cmake_minimum_required(VERSION 3.10)
file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.hpp)

project(uActorLinux C CXX)

find_package(Threads REQUIRED)
add_executable(uActorBin ${SOURCES} ${HEADERS})
target_include_directories(uActorBin PUBLIC "include")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)

target_compile_options(uActorBin PRIVATE -std=c++17)
target_link_libraries(uActorBin uActor)
target_link_libraries(uActorBin Threads::Threads)