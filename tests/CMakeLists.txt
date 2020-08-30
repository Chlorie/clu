# Unit test

enable_testing()

set(APP_NAME tests)

set(SOURCE_FILES
    "main.cpp"
)

add_executable(${APP_NAME} ${SOURCE_FILES})

target_compile_features(${APP_NAME} PRIVATE cxx_std_17) # Use C++17
set_target_properties(${APP_NAME} PROPERTIES CXX_EXTENSIONS off)

if (MSVC)
    # Force MSVC to use utf-8 encoding regardless of whether the BOM exists
    target_compile_options(${APP_NAME} PRIVATE "/utf-8")
    # /Zc:__cplusplus is needed for MSVC to produce correct value for the __cplusplus macro
    # If this option is not on, __cplusplus will always expands to 199711L
    if (MSVC_VERSION GREATER_EQUAL 1914)
        target_compile_options(${APP_NAME} PUBLIC "/Zc:__cplusplus")
    endif ()
endif ()

# Warnings and errors settings
# Use highest reasonable warning level, and treat warnings as errors
if (MSVC) # Visual Studio
    if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]") # If default /W3 presents
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") # Change /W3 to /W4
    else ()
        target_compile_options(${APP_NAME} PRIVATE /W4) # Add /W4 directly
    endif ()
    target_compile_options(${APP_NAME} PRIVATE /WX) # Treat warnings as errors
    # Treat all header files specified by angle brackets to be system headers, and ignore all those warnings
    target_compile_options(${APP_NAME} PRIVATE 
        /experimental:external /external:W0 /external:anglebrackets)
else () # Not Visual Studio, assuming gcc or clang
    target_compile_options(${APP_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
endif ()

find_package(GTest CONFIG REQUIRED)
target_link_libraries(${APP_NAME} PRIVATE
    clu
    GTest::gtest_main
)

add_test(AllTestsInMain ${APP_NAME})
