add_library(clu INTERFACE)
add_library(clu::clu ALIAS clu)

target_include_directories(clu
INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

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
