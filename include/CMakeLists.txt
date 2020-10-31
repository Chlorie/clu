# Library (currently header only)

set(LIB_NAME clu)

add_library(${LIB_NAME} INTERFACE)
target_include_directories(${LIB_NAME} INTERFACE "./")
