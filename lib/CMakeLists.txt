add_library(clu)
add_library(clu::clu ALIAS clu)

set_target_properties(clu PROPERTIES LINKER_LANGUAGE CXX)
target_set_output_dirs(clu)

function (add_headers TGT)
    set(headers ${ARGN})
    list(TRANSFORM headers PREPEND "include/clu/")
    set(${TGT} ${headers} PARENT_SCOPE)
endfunction ()
function (add_sources TGT)
    set(sources ${ARGN})
    list(TRANSFORM sources PREPEND "src/")
    set(${TGT} ${sources} PARENT_SCOPE)
endfunction ()

add_headers(HEADERS
    # Header files here (relative to ./include/clu/)
    "async/async_manual_reset_event.h"

    "execution/algorithms.h"
    "execution/any_scheduler.h"
    "execution/awaitable_traits.h"
    "execution/execution_traits.h"
    "execution/run_loop.h"
    "execution/utility.h"

    "any_unique.h"
    "assertion.h"
    "async.h"
    "buffer.h"
    "c_str_view.h"
    "chrono_utils.h"
    "concepts.h"
    "concurrency.h"
    "copy_elider.h"
    "coroutine.h"
    "execution.h"
    "execution_contexts.h"
    "expected.h"
    "export.h"
    "file.h"
    "fixed_string.h"
    "flags.h"
    "forest.h"
    "function_ref.h"
    "function_traits.h"
    "generator.h"
    "hash.h"
    "indices.h"
    "integer_literals.h"
    "invocable_wrapper.h"
    "iterator.h"
    "macros.h"
    "manual_lifetime.h"
    "meta_algorithm.h"
    "meta_list.h"
    "new.h"
    "oneway_task.h"
    "optional_ref.h"
    "overload.h"
    "parse.h"
    "piper.h"
    "polymorphic_value.h"
    "polymorphic_visit.h"
    "random.h"
    "scope.h"
    "semver.h"
    "static_for.h"
    "static_vector.h"
    "stop_token.h"
    "string_utils.h"
    "tag_invoke.h"
    "take.h"
    "task.h"
    "type_traits.h"
    "unique_coroutine_handle.h"
    "uuid.h"
    "vector_utils.h"
)
add_sources(SOURCES
    # Source files here (relative to ./src/)
    "concurrency.cpp"
    "file.cpp"
    "random.cpp"
    "semver.cpp"
    "stop_token.cpp"
    "uuid.cpp"
)

target_sources(clu PRIVATE ${HEADERS} ${SOURCES})
target_set_options(clu PRIVATE)
target_set_warnings(clu PRIVATE)
target_set_cxx_std(clu)

if (BUILD_SHARED_LIBS)
    target_compile_definitions(clu
        PRIVATE CLU_EXPORT_SHARED
        PUBLIC CLU_BUILD_SHARED)
endif ()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

target_include_directories(clu
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(clu PUBLIC Threads::Threads)

if (ENABLE_INSTALL)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/cluConfigVersion.cmake"
        VERSION ${PACKAGE_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(TARGETS clu
        EXPORT cluTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        PUBLIC_HEADER DESTINATION include
    )

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/cluConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/cluConfig.cmake"
        INSTALL_DESTINATION lib/cmake/clu
    )

    export(TARGETS clu NAMESPACE clu:: FILE cluConfig.cmake)
    install(EXPORT cluTargets NAMESPACE clu:: DESTINATION lib/cmake/clu)
    install(FILES "${PROJECT_BINARY_DIR}/cluConfigVersion.cmake"
                  "${PROJECT_BINARY_DIR}/cluConfig.cmake"
            DESTINATION lib/cmake/clu)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/lib/include/ DESTINATION include)
endif ()
