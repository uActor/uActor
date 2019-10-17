idf_component_register(
  SRCS "main.cpp"
  INCLUDE_DIRS ""
  REQUIRES "lua"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)