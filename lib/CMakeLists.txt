add_library(clu)
add_library(clu::clu ALIAS clu)

set_target_properties(clu PROPERTIES
    LINKER_LANGUAGE CXX
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
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
    "any_unique.h"
    "assertion.h"
    "async.h"
    "box.h"
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
    "file.h"
    "fixed_string.h"
    "flags.h"
    "flow.h"
    "forest.h"
    "function_ref.h"
    "function_traits.h"
    "functional.h"
    "generator.h"
    "hash.h"
    "indices.h"
    "integer_literals.h"
    "intrusive_list.h"
    "invocable_wrapper.h"
    "iterator.h"
    "macros.h"
    "manual_lifetime.h"
    "memory.h"
    "memory_resource.h"
    "meta_algorithm.h"
    "meta_list.h"
    "new.h"
    "oneway_task.h"
    "optional_ref.h"
    "ordered_hash_set.h"
    "overload.h"
    "parse.h"
    "piper.h"
    "polymorphic_value.h"
    "polymorphic_visit.h"
    "query.h"
    "random.h"
    "scope.h"
    "semver.h"
    "static_for.h"
    "static_vector.h"
    "stop_token.h"
    "string_utils.h"
    "tag_invoke.h"
    "task.h"
    "text.h"
    "type_traits.h"
    "unique_coroutine_handle.h"
    "uri.h"
    "uuid.h"
    "vector_utils.h"

    "async/lock.h"
    "async/manual_reset_event.h"
    "async/mutex.h"
    "async/scope.h"
    "async/shared_mutex.h"

    "concurrency/hazard_pointer.h"
    "concurrency/locked.h"
    "concurrency/locked_ptr.h"
    "concurrency/spinlock.h"

    "execution/any_scheduler.h"
    "execution/awaitable_traits.h"
    "execution/execution_traits.h"
    "execution/factories.h"
    "execution/run_loop.h"
    "execution/schedulers.h"
    "execution/utility.h"

    "execution/algorithms.h"
    "execution/algorithms/basic.h"
    "execution/algorithms/composed.h"
    "execution/algorithms/consumers.h"
    "execution/algorithms/parallel.h"
    "execution/algorithms/scheduling.h"
    "execution/algorithms/trivial_streams.h"
    "execution/algorithms/stream_basic.h"

    "execution_contexts/threads.h"
    "execution_contexts/timed_threads.h"

    "text/print.h"
    "text/utf_convert.h"
)
add_sources(SOURCES
    # Source files here (relative to ./src/)
    "assertion.cpp"
    "file.cpp"
    "random.cpp"
    "semver.cpp"
    "stop_token.cpp"
    "uri.cpp"
    "uuid.cpp"

    "async/manual_reset_event.cpp"
    "async/mutex.cpp"
    "async/scope.cpp"
    "async/shared_mutex.cpp"
    
    "concurrency/hazard_pointer.cpp"
    "concurrency/locked_ptr.cpp"
    "concurrency/spinlock.cpp"

    "execution/run_loop.cpp"
    "execution/schedulers.cpp"
    
    "execution_contexts/threads.cpp"
    "execution_contexts/timed_threads.cpp"

    "text/print.cpp"
    "text/utf_convert.cpp"
)

target_sources(clu PRIVATE ${HEADERS} ${SOURCES})
target_set_options(clu PRIVATE)
target_set_warnings(clu PRIVATE)
target_set_cxx_std(clu)

if (MSVC)
    set(CLU_NATVIS_DIR "misc/natvis/")
    set(CLU_NATVIS_FILE "${CLU_NATVIS_DIR}clu.natvis")
    target_sources(clu INTERFACE
        $<INSTALL_INTERFACE:${CLU_NATVIS_FILE}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${CLU_NATVIS_FILE}>
    )
    target_link_options(clu PRIVATE
        $<INSTALL_INTERFACE:/NATVIS:${CLU_NATVIS_FILE}>
        $<BUILD_INTERFACE:/NATVIS:${CMAKE_CURRENT_SOURCE_DIR}/${CLU_NATVIS_FILE}>
    )
endif ()

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
install(DIRECTORY "${PROJECT_SOURCE_DIR}/lib/include/" DESTINATION "include")

if (MSVC)
    install(FILES ${CLU_NATVIS_FILE} DESTINATION ${CLU_NATVIS_DIR})
endif()
