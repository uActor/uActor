cmake_minimum_required(VERSION 3.14)

project(rt_unit_test)

set(CMAKE_CXX_STANDARD 17)
# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# set(CONFIG_UACTOR_V8 ON)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(googletest EXCLUDE_FROM_ALL)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../uActor ${CMAKE_CURRENT_BINARY_DIR}/uActor)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()

include_directories(../main/legacy/include)

add_executable(uactor_test ../uActor-POSIX/src/receiver.cpp mock_board_functions.cpp pubsub_tests.cpp runtime_tests.cpp forwarding_tests.cpp routing_rule_test.cpp constraint_test.cpp)
target_link_libraries(uactor_test gtest_main)
target_link_libraries(uactor_test uActor)
add_test(NAME uactor_test COMMAND uactor_test)