option(ENABLE_IPO "Enable interprocedural optimization (link-time optimization)" ON)
option(ENABLE_CXX_EXTENSIONS "Enable C++ language extensions" OFF)

if (ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(
        RESULT result
        OUTPUT output)
    if (result)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else ()
        message(SEND_ERROR "IPO is not supported: ${output}")
    endif ()
endif ()

set(CMAKE_CXX_EXTENSIONS ${ENABLE_CXX_EXTENSIONS})

function (target_set_warnings TGT ACCESS)
    option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

    set(MSVC_WARNINGS
        /W4 # Highest reasonable value
        /w14242 /w14254 /w14263 /w14265 /w14287
        /we4289 /w14296 /w14311 /w14545 /w14546
        /w14547 /w14549 /w14555 /w14619 /w14640
        /w14826 /w14905 /w14906 /w14928)

    set(MSVC_SUPPRESS_EXTERNAL_WARNINGS
        # Ignore warnings from external includes
        /experimental:external
        /external:W0 /external:anglebrackets /external:templates-)

    set(CLANG_WARNINGS
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wformat=2)

    if (WARNINGS_AS_ERRORS)
        set(MSVC_WARINGS ${MSVC_WARNINGS} /WX)
        set(CLANG_WARNINGS ${CLANG_WARNINGS} -Werror)
    endif ()

    set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wmisleading-indentation -Wduplicated-cond
        -Wduplicated-branches -Wlogical-op)
        
    if (MSVC) # MSVC-like
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # clang-cl
            target_compile_options(${TGT} ${ACCESS} ${MSVC_WARNINGS})
        else () # Real MSVC
            string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") # Remove default /W3
            target_compile_options(${TGT} ${ACCESS} ${MSVC_WARNINGS} ${MSVC_SUPPRESS_EXTERNAL_WARNINGS})
        endif ()
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # clang
        target_compile_options(${TGT} ${ACCESS} ${CLANG_WARNINGS})
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # gcc
        target_compile_options(${TGT} ${ACCESS} ${GCC_WARNINGS})
    else()
        message(AUTHOR_WARNING "No compiler warnings set for '${CMAKE_CXX_COMPILER_ID}' compiler.")
    endif ()
endfunction ()

function (target_set_options TGT ACCESS)
    # Asio requires _WIN32_WINNT to be set
    if (WIN32)
        if (${CMAKE_SYSTEM_VERSION} EQUAL 10) # Windows 10
            set(WIN32_WINNT 0x0A00)
        elseif (${CMAKE_SYSTEM_VERSION} EQUAL 6.3) # Windows 8.1
            set(WIN32_WINNT 0x0603)
        elseif (${CMAKE_SYSTEM_VERSION} EQUAL 6.2) # Windows 8
            set(WIN32_WINNT 0x0602)
        elseif (${CMAKE_SYSTEM_VERSION} EQUAL 6.1) # Windows 7
            set(WIN32_WINNT 0x0601)
        elseif (${CMAKE_SYSTEM_VERSION} EQUAL 6.0) # Windows Vista
            set(WIN32_WINNT 0x0600)
        else () # Windows XP (5.1)
            set(WIN32_WINNT 0x0501)
        endif ()
        string(TOUPPER ${ACCESS} ACCESS)
        if (ACCESS STREQUAL "INTERFACE")
            target_compile_definitions(${TGT} INTERFACE _WIN32_WINNT=${WIN32_WINNT})
        else ()
            target_compile_definitions(${TGT} PUBLIC _WIN32_WINNT=${WIN32_WINNT})
        endif ()
    endif ()

    if (MSVC) # Visual Studio
        if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(${TGT} ${ACCESS} # Conformance settings
                /utf-8 /permissive- /Zc:__cplusplus /Zc:externConstexpr)
        endif ()
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${TGT} ${ACCESS}
            -stdlib=libc++
            -fcolor-diagnostics
            -ftemplate-backtrace-limit=32)
        target_link_options(${TGT} ${ACCESS} -stdlib=libc++)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${TGT}
            ${ACCESS}
            -fdiagnostics-color=always
            -fconcepts-diagnostics-depth=16
            -ftemplate-backtrace-limit=32)
    endif ()
endfunction ()

function (target_set_output_dirs TGT)
    set_target_properties(${TGT} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")
endfunction ()

function (target_set_cxx_std TGT)
    # Set C++20
    if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129)
        # We actually need /std:c++latest (C++23) for some of the C++20 features
        # i.e. ranges and format, for MSVC 16.11 or later
        target_compile_features(${TGT} PRIVATE cxx_std_23)
    else ()
        target_compile_features(${TGT} PRIVATE cxx_std_20)
    endif ()
endfunction ()
