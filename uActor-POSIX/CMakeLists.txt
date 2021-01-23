cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
add_compile_options(-Werror -Wall -Wextra -pedantic)

target_compile_options(uActor PUBLIC -DCONFIG_BENCHMARK_ENABLED=1)

if (NOT TARGET InfluxData::InfluxDB)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/influxdb-cxx ${CMAKE_CURRENT_BINARY_DIR}/lib_influxdb)
endif()
target_link_libraries(uActorBin InfluxData::InfluxDB)

target_link_libraries(uActorBin uActor)

target_link_libraries(uActorBin Threads::Threads)
target_link_libraries(uActorBin Boost::program_options)