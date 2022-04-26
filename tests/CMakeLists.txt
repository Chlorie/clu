# Unit test

enable_testing()

find_package(Catch2 CONFIG REQUIRED)

add_executable(tests
    "main.cpp" # with CATCH_CONFIG_MAIN

    "async.cpp"
    "expected.cpp"
    "flags.cpp"
    "forest.cpp"
    "function_ref.cpp"
    "generator.cpp"
    "hash.cpp"
    "indices.cpp"
    "meta_algorithm.cpp"
    "overload.cpp"
    "parse.cpp"
    "polymorphic_value.cpp"
    "scope.cpp"
    "semver.cpp"
    "static_vector.cpp"
    "string_utils.cpp"
    "tag_invoke.cpp"
    "uri.cpp"
    "uuid.cpp"

    "execution/algorithms.cpp"
)
target_link_libraries(tests PRIVATE
    clu
    project_options
    Catch2::Catch2
)
target_set_cxx_std(tests)
add_test(AllTestsInMain tests)
