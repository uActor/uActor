set(CMAKE_SYSTEM_NAME wasp)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR wasm32)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_C_COMPILER_TARGET wasm32)
set(CMAKE_CXX_COMPILER_TARGET wasm32)
set(CMAKE_C_FLAGS "-flto -fvisibility=hidden -ffunction-sections -fdata-sections -nostdlib" CACHE INTERNAL "")
set(CMAKE_CXX_FLAGS "-flto -fvisibility=hidden -ffunction-sections -fdata-sections -nostdlib" CACHE INTERNAL "")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-entry -Wl,--export-dynamic -Wl,--allow-undefined -Wl,-z,stack-size=4096" CACHE INTERNAL "")
set(CMAKE_C_OUTPUT_EXTENSION ".bc" CACHE INTERNAL "")
set(CMAKE_CXX_OUTPUT_EXTENSION ".bc" CACHE INTERNAL "")
set(CMAKE_EXECUTABLE_SUFFIX_C ".wat" CACHE INTERNAL "")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".wat" CACHE INTERNAL "")

add_compile_definitions(WASM32)

# Building shared libraries is not supported with wasm
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
# Creating non Release versions creates Problems
set(CMAKE_BUILD_TYPE Release CACHE INTERNAL "")
