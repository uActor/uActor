file(GLOB_RECURSE SOURCES lua/*.c)
file(GLOB_RECURSE HEADER lua/*.hpp include)

add_library(lib_lua STATIC ${SOURCES} ${HEADERS})
target_include_directories(lib_lua PUBLIC "lua/" "include")

target_compile_options(lib_lua PUBLIC -DLUA_32BITS)