# Include V8 built externaly as described in the embedders guide: https://v8.dev/docs/embed.
# V8 does not support CMake, so integrating its build will require further work.
# TODO(raphaelhetzel) We should perfrom the build from CMake.

# Build configurations are included (gn_args based on the sample configuration, Linux) or use the sample defaults.
# The build is automated using get_v8_linux.sh and get_v8_mac.sh.
# https://www.mail-archive.com/v8-dev@googlegroups.com/msg160977.html

# Resources related to the CMake integration:
# https://stackoverflow.com/q/25907478
# https://stackoverflow.com/questions/44859161/compiling-a-c-program-with-v8-library-with-cmake
# http://www.saoe.net/blog/using-cmake-with-external-projects/
# https://v8.dev/docs/cross-compile-arm

set(PREBUILT_V8_VERSION "9.8.109")

include(FetchContent)
FetchContent_Declare(
  v8
  GIT_REPOSITORY https://chromium.googlesource.com/v8/v8
  GIT_TAG       "refs/tags/${PREBUILT_V8_VERSION}"
  GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(v8)

# TODO(raphaelhetzel) The name in the toolchain configs might be wrong, check and adapt this
if(CMAKE_SYSTEM_PROCESSOR STREQUAL arm)
  set(PREBUILT_V8_ARCH arm)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL armv8)
  set(PREBUILT_V8_ARCH arm64)
else()
  set(PREBUILT_V8_ARCH x64)
endif()

if(CMAKE_HOST_APPLE)
set(PREBUILT_V8_OS macos)
else()
set(PREBUILT_V8_OS linux)
endif()

# The global option is required to propagate the headers: https://stackoverflow.com/a/48355969
add_library(lib_v8 STATIC IMPORTED GLOBAL)
set_property(TARGET lib_v8 PROPERTY IMPORTED_LOCATION
   "${CMAKE_CURRENT_LIST_DIR}/precompiled/${PREBUILT_V8_VERSION}/${PREBUILT_V8_OS}_${PREBUILT_V8_ARCH}_libv8_monolith.a"
)   
if(NOT PREBUILT_V8_ARCH STREQUAL arm)
target_compile_definitions(lib_v8 INTERFACE V8_COMPRESS_POINTERS=1)
endif()
target_include_directories(lib_v8 INTERFACE "${v8_SOURCE_DIR}/include")