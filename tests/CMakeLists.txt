# Unit test

enable_testing()

find_package(Catch2 CONFIG REQUIRED)
include(CTest)
include(Catch)

function (add_test_executable TGT)
    add_executable("test.${TGT}")
    target_sources("test.${TGT}" PRIVATE ${ARGN})
    target_link_libraries("test.${TGT}" PRIVATE
        project_options
        clu
        Catch2::Catch2WithMain
    )
    target_set_cxx_std("test.${TGT}")
    target_set_output_dirs("test.${TGT}")
    catch_discover_tests("test.${TGT}")
endfunction ()

add_test_executable(async "async.cpp")
add_test_executable(expected "expected.cpp")
add_test_executable(flags "flags.cpp")
add_test_executable(forest "forest.cpp")
add_test_executable(function_ref "function_ref.cpp")
add_test_executable(generator "generator.cpp")
add_test_executable(hash "hash.cpp")
add_test_executable(indices "indices.cpp")
add_test_executable(meta_algorithm "meta_algorithm.cpp")
add_test_executable(overload "overload.cpp")
add_test_executable(parse "parse.cpp")
add_test_executable(polymorphic_value "polymorphic_value.cpp")
add_test_executable(scope "scope.cpp")
add_test_executable(semver "semver.cpp")
add_test_executable(static_vector "static_vector.cpp")
add_test_executable(string_utils "string_utils.cpp")
add_test_executable(tag_invoke "tag_invoke.cpp")
add_test_executable(uri "uri.cpp")
add_test_executable(uuid "uuid.cpp")

add_test_executable(execution.algorithms "execution/algorithms.cpp")
