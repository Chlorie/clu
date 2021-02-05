# Chlorie's utilities library

cmake_minimum_required(VERSION 3.14)

project(clu LANGUAGES CXX VERSION 0.14.3)

set(LIB_NAME clu)

option(CLU_BUILD_EXAMPLES "Build examples" OFF)
option(CLU_INSTALL "Install the library" OFF)

function (target_set_options TGT)
    # Set C++20
    target_compile_features(${TGT} PRIVATE cxx_std_20)
    set_target_properties(${TGT} PROPERTIES
        CXX_EXTENSIONS OFF
        INTERPROCEDURAL_OPTIMIZATION ON)

    # MSVC settings
    if (MSVC)
        # Force MSVC to use utf-8 encoding regardless of whether the BOM exists
        target_compile_options(${TGT} PRIVATE "/utf-8")
        # /Zc:__cplusplus is needed for MSVC to produce correct value for the __cplusplus macro
        # If this option is not on, __cplusplus will always expands to 199711L
        if (MSVC_VERSION GREATER_EQUAL 1914)
            target_compile_options(${TGT} PUBLIC "/Zc:__cplusplus")
        endif ()
    endif ()

    # Warnings and errors settings
    # Use highest reasonable warning level, and treat warnings as errors
    if (MSVC) # Visual Studio
        if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]") # If default /W3 presents
            string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") # Change /W3 to /W4
        else ()
            target_compile_options(${TGT} PRIVATE /W4) # Add /W4 directly
        endif ()
        target_compile_options(${TGT} PRIVATE /WX) # Treat warnings as errors
        # Treat all header files specified by angle brackets to be system headers, and ignore all those warnings
        target_compile_options(${TGT} PRIVATE 
            /experimental:external /external:W0 /external:anglebrackets)
    else () # Not Visual Studio, assuming gcc or clang
        target_compile_options(${TGT} PRIVATE
            -Wall -Wextra -pedantic -Wundef -Wconversion -Wzero-as-null-pointer-constant -Werror)
    endif ()
endfunction ()

# Library (currently header only)

add_library(clu INTERFACE)
target_include_directories(clu INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# Options

if (CLU_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (CLU_INSTALL)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/${LIB_NAME}ConfigVersion.cmake"
        VERSION ${PACKAGE_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(TARGETS ${LIB_NAME}
        EXPORT ${LIB_NAME}Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        PUBLIC_HEADER DESTINATION include
    )

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/${LIB_NAME}Config.cmake.in"
        "${PROJECT_BINARY_DIR}/${LIB_NAME}Config.cmake"
        INSTALL_DESTINATION lib/cmake/${LIB_NAME}
    )

    install(EXPORT ${LIB_NAME}Targets DESTINATION lib/cmake/${LIB_NAME})
    install(FILES "${PROJECT_BINARY_DIR}/${LIB_NAME}ConfigVersion.cmake"
                  "${PROJECT_BINARY_DIR}/${LIB_NAME}Config.cmake"
            DESTINATION lib/cmake/${LIB_NAME})
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)
endif ()
