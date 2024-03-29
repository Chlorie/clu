# Unit test

find_package(Catch2 CONFIG REQUIRED)
include(CTest)
include(Catch)

function (add_test_target FILE)
    string(REPLACE "/" "." FILE_DOTTED ${FILE})
    set(TGT "test.${FILE_DOTTED}")
    add_executable(${TGT} "${FILE}.cpp")
    set_target_properties(${TGT} PROPERTIES FOLDER tests)
    target_link_libraries(${TGT} PRIVATE
        project_options
        clu
        Catch2::Catch2WithMain
    )
    target_set_cxx_std(${TGT})
    target_set_output_dirs(${TGT})
    catch_discover_tests(${TGT})
endfunction ()

add_test_target("async")
add_test_target("box")
add_test_target("event")
add_test_target("expected")
add_test_target("flags")
add_test_target("forest")
add_test_target("function")
add_test_target("function_ref")
add_test_target("generator")
add_test_target("hash")
add_test_target("indices")
add_test_target("intrusive_list")
add_test_target("meta_algorithm")
add_test_target("ordered_hash_set")
add_test_target("overload")
add_test_target("parse")
add_test_target("polymorphic_value")
add_test_target("query")
add_test_target("rational")
add_test_target("scope")
add_test_target("semver")
add_test_target("static_vector")
add_test_target("string_utils")
add_test_target("tag_invoke")
add_test_target("uri")
add_test_target("uuid")
add_test_target("execution/algorithms/basic")
add_test_target("execution/algorithms/composed")
add_test_target("execution/algorithms/consumers")
add_test_target("execution/algorithms/parallel")
add_test_target("execution/algorithms/scheduling")
