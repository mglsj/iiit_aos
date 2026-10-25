cmake_minimum_required(VERSION 3.13)

project(a1 C)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompilerFlags.cmake)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(WARNINGS -Wall -Wextra)

add_compile_options(${WARNINGS})
enable_sanitizer()

foreach(q q1 q2)
    add_executable(${q} ${q}.c)
    target_link_libraries(${q} PRIVATE m)
endforeach()

write_compile_flags(LANG C STD gnu17 FLAGS ${WARNINGS})
