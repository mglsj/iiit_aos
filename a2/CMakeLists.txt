cmake_minimum_required(VERSION 4.4)

project(shell LANGUAGES C)

include(${PROJECT_SOURCE_DIR}/../cmake/CompilerFlags.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(WARNINGS -Wall -Wextra)

add_library(options INTERFACE)
target_compile_features(options INTERFACE c_std_23)
target_compile_options(options INTERFACE ${WARNINGS})
enable_sanitizer(options)
enable_hardening(options)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.c)

add_executable(shell)
target_sources(shell PRIVATE ${SOURCE_FILES})
target_link_libraries(shell PRIVATE options readline)

find_path(READLINE_INCLUDE_DIR readline/readline.h)
write_compile_flags(
    LANG C
    STD gnu23
    FLAGS ${WARNINGS}
    INCLUDES ${READLINE_INCLUDE_DIR}
)
