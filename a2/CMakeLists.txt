cmake_minimum_required(VERSION 3.13)

project(shell)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(shell ${SOURCE_FILES})

target_compile_options(shell PRIVATE -Wall -Wextra)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(
        shell PRIVATE -fsanitize=address -fno-omit-frame-pointer -g
    )
    target_link_options(shell PRIVATE -fsanitize=address)
endif()

target_link_libraries(shell PRIVATE readline)
