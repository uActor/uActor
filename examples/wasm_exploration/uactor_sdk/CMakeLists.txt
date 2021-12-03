cmake_minimum_required(VERSION 3.16)
project(uactor_sdk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_library(uactor_sdk STATIC)
target_include_directories(uactor_sdk PUBLIC include)

add_subdirectory(src)
