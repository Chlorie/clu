# Documentation generator

find_package(Doxygen REQUIRED)
find_package(Sphinx REQUIRED)

file(GLOB_RECURSE PUBLIC_HEADERS "../lib/include/*/*.h")

set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

# Only re-generate Doxygen when Doxyfile or public headers change
file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})
add_custom_command(
    OUTPUT ${DOXYGEN_INDEX_FILE}
    DEPENDS ${PUBLIC_HEADERS}
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
    MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
    COMMENT "Generating docs"
    VERBATIM)

add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})

file(GLOB_RECURSE RST_DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*/*.rst")

set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
set(SPHINX_BUILD ${PROJECT_SOURCE_DIR}/docs/output)
set(SPHINX_INDEX_FILE ${SPHINX_BUILD}/index.html)

# Only re-generate Sphinx when:
# - Doxygen has rerun
# - Our doc files have been updated
# - The sphinx config has been updated
add_custom_command(
    OUTPUT ${SPHINX_INDEX_FILE}
    COMMAND
        ${SPHINX_EXECUTABLE} -b html
        # Tell Breathe where to find the Doxygen output
        -Dbreathe_projects.clu=${DOXYGEN_OUTPUT_DIR}/xml
    ${SPHINX_SOURCE} ${SPHINX_BUILD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/index.rst"
        ${DOXYGEN_INDEX_FILE}
    MAIN_DEPENDENCY ${SPHINX_SOURCE}/conf.py
    COMMENT "Generating documentation with Sphinx")

add_custom_target(Sphinx ALL DEPENDS ${SPHINX_INDEX_FILE})

# Don't build the docs when building "all"
set_target_properties(
    Doxygen Sphinx
PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    FOLDER docs
)
