cmake_minimum_required(VERSION 3.13)

project(shell C)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompilerFlags.cmake)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.c src/*.h)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(WARNINGS -Wall -Wextra)

add_compile_options(${WARNINGS})
enable_sanitizer()

add_executable(shell ${SOURCE_FILES})
target_link_libraries(shell PRIVATE readline)

find_path(READLINE_INCLUDE_DIR readline/readline.h)
write_compile_flags(
    LANG C
    STD gnu2x
    FLAGS ${WARNINGS}
    INCLUDES ${READLINE_INCLUDE_DIR}
)
