# Chlorie's utilities library

cmake_minimum_required(VERSION 3.12)

project(clu)

option(CLU_BUILD_EXAMPLES "Build examples" OFF)

add_subdirectory(include)

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

if (CLU_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
