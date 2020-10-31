# Examples

function (add_example TARGET_NAME FILE)
    add_executable(${TARGET_NAME} ${FILE})
    target_set_options(${TARGET_NAME})
    target_link_libraries(${TARGET_NAME} PRIVATE clu)
endfunction ()

add_example(scope scope.cpp)
add_example(partial partial.cpp)
