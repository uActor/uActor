cmake_minimum_required(VERSION 3.15)

project(rt_unit_test)

set(CMAKE_CXX_STANDARD 17)
# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(googletest EXCLUDE_FROM_ALL)

file(GLOB lua_SRC
  "../components/lua/lua/*.c"
)
file(GLOB executable "../components/lua/lua/lua.c")
message(${lua_SRC})
list(REMOVE_ITEM lua_SRC ${executable})

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()
include_directories(../components/lua/include)
include_directories(../main/include)

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(runtime_system_test ${lua_SRC} ../main/managed_actor.cpp mock_board_functions.cpp mock_router.cpp test.cpp)
target_link_libraries(runtime_system_test gtest_main)
add_test(NAME runtime_system_unit_test COMMAND runtime_system_test)