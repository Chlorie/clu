# Unit test

enable_testing()

find_package(GTest CONFIG REQUIRED)

add_executable(tests
    "expected.cpp"
    "flags.cpp"
    "forest.cpp"
    "function_ref.cpp"
    "functional.cpp"
    # "generator.cpp"
    "hash.cpp"
    "indices.cpp"
    "meta_algorithm.cpp"
    "pipeable.cpp"
    "polymorphic_value.cpp"
    "scope.cpp"
    "static_vector.cpp"
    "string_utils.cpp"
    "take.cpp"
    "wide_integer.cpp"
)
target_link_libraries(tests PRIVATE
    clu
    project_options
    GTest::gmock_main
)
add_test(AllTestsInMain tests)
