set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv8)

set(CMAKE_SYSTEM_ROOT /usr/aarch64-linux-gnu)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc-10)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++-10)

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
