# Chlorie's utilities library

cmake_minimum_required(VERSION 3.20)

project(clu LANGUAGES CXX VERSION 0.38.0)

set(CMAKE_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SCRIPTS})

add_library(project_options INTERFACE)
include("cmake/ProjectSettings.cmake")
target_set_warnings(project_options INTERFACE)
target_set_options(project_options INTERFACE)

option(BUILD_SHARED_LIBS "Build library as shared library" OFF)

add_subdirectory(lib)

option(CLU_BUILD_EXAMPLES "Build code examples" ON)
if (CLU_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

option(CLU_BUILD_DOCS "Build documentation using Doxygen & Sphinx" OFF)
if (CLU_BUILD_DOCS)
    add_subdirectory(docs)
endif ()

option(CLU_BUILD_TESTS "Build unit tests" OFF)
if (CLU_BUILD_TESTS)
    list(APPEND VCPKG_MANIFEST_FEATURES "unit-test")
    add_subdirectory(tests)
endif ()
