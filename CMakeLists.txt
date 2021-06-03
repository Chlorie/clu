# Chlorie's utilities library

cmake_minimum_required(VERSION 3.14)

project(clu LANGUAGES CXX VERSION 0.26.0)

set(CMAKE_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SCRIPTS})

add_library(project_options INTERFACE)
include("cmake/ProjectSettings.cmake")
target_compile_features(project_options INTERFACE cxx_std_20)
target_set_warnings(project_options INTERFACE)
target_set_options(project_options INTERFACE)

option(ENABLE_INSTALL "Enable to install the project" ON)

add_subdirectory(lib)

option(BUILD_EXAMPLES "Build code examples" ON)
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

option(BUILD_TESTS "Build unit tests" OFF)
if (BUILD_TESTS)
    list(APPEND VCPKG_MANIFEST_FEATURES "unit-test")
    add_subdirectory(tests)
endif ()
