project(HTTPServer)
add_executable(server main.c)

# if (MSVC)
#     target_compile_features(server PRIVATE c_std_23)
# else()
    target_compile_features(server PRIVATE c_std_99)
# endif()

if (MSVC)
    target_compile_options(server PRIVATE /W4)
else()
    target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -ggdb)
endif()

if (WIN32)
    target_link_libraries(server PRIVATE wsock32)
endif()