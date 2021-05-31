# Unit test

enable_testing()

find_package(GTest CONFIG REQUIRED)

function (add_test_exe TARGET_NAME)
    add_executable(${TARGET_NAME} "${TARGET_NAME}.cpp")
    target_set_options(${TARGET_NAME})
    target_link_libraries(${TARGET_NAME} PRIVATE
        clu
        GTest::gmock_main
    )
    add_test(AllTestsInMain ${TARGET_NAME})
endfunction ()

add_executable(tests
    "flags.cpp"
    "forest.cpp"
    "meta_algorithm.cpp"
    "polymorphic_value.cpp"
    "static_vector.cpp"
    "take.cpp"
    "wide_integer.cpp"
)
target_set_options(tests)
target_link_libraries(tests PRIVATE
    clu
    GTest::gmock_main
)
add_test(AllTestsInMain tests)
