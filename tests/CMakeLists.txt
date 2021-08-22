# Unit test

enable_testing()

find_package(Catch2 CONFIG REQUIRED)

add_executable(tests
    "main.cpp" # with CATCH_CONFIG_MAIN
    "expected.cpp"
    "flags.cpp"
    "forest.cpp"
    "function_ref.cpp"
    "functional.cpp"
    # "generator.cpp"
    "hash.cpp"
    "indices.cpp"
    "meta_algorithm.cpp"
    "overload.cpp"
    "parse.cpp"
    "pipeable.cpp"
    "polymorphic_value.cpp"
    "scope.cpp"
    "semver.cpp"
    "static_vector.cpp"
    "string_utils.cpp"
    "tag_invoke.cpp"
    "take.cpp"
    "uuid.cpp"
    "wide_integer.cpp")
target_link_libraries(tests PRIVATE
    clu
    project_options
    Catch2::Catch2
)
target_compile_features(tests PRIVATE cxx_std_20)
add_test(AllTestsInMain tests)
