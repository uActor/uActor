cmake_minimum_required(VERSION 3.10)
project(uActorLinux C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CONFIG_UACTOR_V8 OFF)

# TODO(raphaelhetzel) this needs to be reenabled, requires code updates, was previsously in the wrong location in the configuration file
#add_compile_options(-Werror -Wall -Wextra -pedantic)

option(BUILD_SHARED_LIBS "Build shared versions of libraries" OFF)
option(HTTPCLIENT_ACTOR_ENABLE_SSL "Enable openssl for the http client actor" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})


file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.hpp)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.16.1/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                TLS_VERIFY ON)
endif()

if (HTTPCLIENT_ACTOR_ENABLE_SSL)
  if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(SSL_BACKEND darwinssl)
  else()
    set(SSL_BACKEND openssl)
  endif()
else()
  set(SSL_BACKEND False)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)
conan_cmake_configure(REQUIRES 
                boost/1.75.0
                libcurl/7.76.0
                uwebsockets/19.2.0
                fmt/7.1.3
                      GENERATORS cmake_find_package
                      OPTIONS
                        boost:shared=False
                        boost:without_atomic=True
                        boost:without_chrono=True
                        boost:without_container=True
                        boost:without_context=True
                        boost:without_contract=True
                        boost:without_coroutine=True
                        boost:without_date_time=True
                        boost:without_exception=False
                        boost:without_fiber=True
                        boost:without_filesystem=True
                        boost:without_graph=True
                        boost:without_graph_parallel=True
                        boost:without_iostreams=True
                        boost:without_json=True
                        boost:without_locale=True
                        boost:without_log=True
                        boost:without_math=True
                        boost:without_mpi=True
                        boost:without_nowide=True
                        boost:without_program_options=False
                        boost:without_python=True
                        boost:without_random=True
                        boost:without_regex=True
                        boost:without_serialization=True
                        boost:without_stacktrace=True
                        boost:without_system=False
                        boost:without_test=False
                        boost:without_thread=True
                        boost:without_timer=True
                        boost:without_type_erasure=True
                        boost:without_wave=True
                        libcurl:shared=False
                        libcurl:with_ssl=${SSL_BACKEND}
                     )

if(CMAKE_SYSTEM_PROCESSOR STREQUAL arm)
  set(CONAN_SYSTEM_PROCESSOR armv7)
else()
  set(CONAN_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL arm)
  set(CONAN_HOST_SYSTEM_PROCESSOR armv7)
else()
  set(CONAN_HOST_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set(CONAN_SYSTEM_NAME Macos)
else()
  set(CONAN_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL Darwin)
  set(CONAN_HOST_SYSTEM_NAME Macos)
else()
  set(CONAN_HOST_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_NAME})
endif()

string(REGEX MATCH "^[0-9]+" MAJOR ${CMAKE_CXX_COMPILER_VERSION})
set(CONAN_COMPILER_VERSION ${MAJOR})

conan_cmake_autodetect(settings)
conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS ${settings}
                    SETTINGS_HOST 
                      arch=${CONAN_SYSTEM_PROCESSOR}
                      os=${CONAN_SYSTEM_NAME}
                      compiler.version=${CONAN_COMPILER_VERSION}
                    SETTINGS_BUILD 
                      ${settings}
                      arch=${CONAN_HOST_SYSTEM_PROCESSOR}
                      os=${CONAN_HOST_SYSTEM_NAME}
                      compiler.version=${CONAN_COMPILER_VERSION} 
                    ENV CC=${CMAKE_C_COMPILER}
                    ENV CXX=${CMAKE_CXX_COMPILER}
                    ENV CFLAGS=${CMAKE_C_FLAGS}
                    ENV CXXFLAGS=${CMAKE_CXX_FLAGS}
                    )

find_package(Threads REQUIRED)

set(usocket_WITH_LIBUV False)
find_package(uwebsockets REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS program_options REQUIRED)
find_package(CURL REQUIRED)
find_package(fmt REQUIRED)

add_executable(uActorBin ${SOURCES} ${HEADERS})
target_include_directories(uActorBin PUBLIC "include")

target_include_directories(uActorBin PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(uActorBin PRIVATE ${usockets_INCLUDE_DIRS}) 
target_include_directories(uActorBin PRIVATE ${uwebsockets_INCLUDE_DIRS})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)

#TODO: Add build configuation utility
# Eval: Disable
target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_ENABLE_TELEMETRY=1)
target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_ENABLE_HEAP_TRACKING=1)
target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_ENABLE_INFLUXDB_ACTOR=1)
target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_ENABLE_HTTP_INGRESS=1)
target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_ENABLE_HTTP_CLIENT_ACTOR=1)
target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_WASM=1)

# Large Scale Eval: Enable
# target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_ENABLE_SECONDS_TELEMETRY=1)

#EVAL: Enable
#target_compile_definitions(uActor PUBLIC CONFIG_BENCHMARK_ENABLED=1)

#Not needed in most situations
#target_compile_definitions(uActor PUBLIC CONFIG_UACTOR_HIBERNATION=1)

if (NOT TARGET InfluxData::InfluxDB)
  set(INFLUXCXX_TESTING OFF CACHE INTERNAL "" FORCE)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../vendor/influxdb-cxx ${CMAKE_CURRENT_BINARY_DIR}/lib_influxdb EXCLUDE_FROM_ALL)
endif()
target_link_libraries(uActorBin InfluxData::InfluxDB)

target_link_libraries(uActorBin uActor)

target_link_libraries(uActorBin CURL::libcurl)

target_link_libraries(uActorBin uwebsockets::uwebsockets)

target_link_libraries(uActorBin Threads::Threads)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1")
target_link_libraries(uActorBin stdc++fs)
endif()

target_link_libraries(uActorBin Boost::program_options)
target_link_libraries(uActorBin fmt::fmt)
