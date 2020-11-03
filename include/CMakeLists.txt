# Library (currently header only)

add_library(clu INTERFACE)
target_include_directories(clu INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:.>)
