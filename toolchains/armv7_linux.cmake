set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CONAN_ARCHITECTURE armv7)

set(CMAKE_SYSTEM_ROOT /usr/arm-linux-gnueabihf)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

#set(COMPILE_FLAGS "-mcpu=cortex-a9 -mfpu=neon -ftree-vectorize")
#set(CMAKE_C_FLAGS ${COMPILE_FLAGS})
#set(CMAKE_CXX_FLAGS ${COMPILE_FLAGS})

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")