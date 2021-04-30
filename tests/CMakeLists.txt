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

add_test_exe(meta_algorithm)
add_test_exe(polymorphic_value)
add_test_exe(static_vector)
add_test_exe(take)
