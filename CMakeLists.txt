project(HTTPServer)
add_executable(server main.c)

# if (MSVC)
#     target_compile_features(server PRIVATE c_std_23)
# else()
    target_compile_features(server PRIVATE c_std_11)
# endif()

if (MSVC)
    target_compile_options(server PRIVATE /W4)
else()
    target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -ggdb)
endif()

if (WIN32)
    target_link_libraries(server PRIVATE wsock32)
endif()

set(dirs
    "img/"
    "book/"
    "text/"
    "json/"
)

foreach(dir ${dirs})
    add_custom_command(TARGET server
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/${dir}"
            "$<TARGET_FILE_DIR:server>/${dir}"
        COMMENT "Copying data"
    )
    message(STATUS "dir=${dir}")
endforeach()