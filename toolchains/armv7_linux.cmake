set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CONAN_ARCHITECTURE armv7)

set(CMAKE_SYSTEM_ROOT /usr/arm-linux-gnueabihf)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc-8)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++-8)

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")