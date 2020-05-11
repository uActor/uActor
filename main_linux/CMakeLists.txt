cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.hpp)

project(uActorLinux C CXX)

find_package(Threads REQUIRED)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS program_options REQUIRED)

add_executable(uActorBin ${SOURCES} ${HEADERS})
target_include_directories(uActorBin PUBLIC "include")

target_include_directories(uActorBin PRIVATE ${Boost_INCLUDE_DIRS}) 

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
target_compile_options(uActorBin PRIVATE -std=c++17)
target_compile_options(uActor PUBLIC -DCONFIG_BENCHMARK_ENABLED=1)

target_link_libraries(uActorBin uActor)


target_link_libraries(uActorBin Threads::Threads)
target_link_libraries(uActorBin Boost::program_options)