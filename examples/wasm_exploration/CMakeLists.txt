cmake_minimum_required(VERSION 3.13)
project(test LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_subdirectory(uactor_sdk)

add_executable(test file.cpp)
target_link_libraries(test PRIVATE uactor_sdk)
