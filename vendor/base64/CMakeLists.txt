add_library(lib_base64 STATIC cpp-base64/base64.cpp cpp-base64/base64.h)
target_compile_options(lib_base64 PRIVATE -std=c++17)
target_include_directories(lib_base64 PUBLIC "cpp-base64/")