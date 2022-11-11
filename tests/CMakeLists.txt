# Unit test

enable_testing()

find_package(Catch2 CONFIG REQUIRED)
include(CTest)
include(Catch)

add_executable(all_tests)
target_sources(all_tests PRIVATE
    "async.cpp"
    "event.cpp"
    "expected.cpp"
    "flags.cpp"
    "forest.cpp"
    "function_ref.cpp"
    "generator.cpp"
    "hash.cpp"
    "indices.cpp"
    "intrusive_list.cpp"
    "meta_algorithm.cpp"
    "ordered_hash_set.cpp"
    "overload.cpp"
    "parse.cpp"
    "polymorphic_value.cpp"
    "rational.cpp"
    "scope.cpp"
    "semver.cpp"
    "static_vector.cpp"
    "string_utils.cpp"
    "tag_invoke.cpp"
    "uri.cpp"
    "uuid.cpp"

    "execution/algorithms.cpp"
)
target_link_libraries(all_tests PRIVATE
    project_options
    clu
    Catch2::Catch2WithMain
)
target_set_cxx_std(all_tests)
target_set_output_dirs(all_tests)
catch_discover_tests(all_tests)
