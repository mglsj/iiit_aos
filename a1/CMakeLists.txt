cmake_minimum_required(VERSION 4.4)

project(a1 LANGUAGES C)

include(${PROJECT_SOURCE_DIR}/../cmake/CompilerFlags.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(WARNINGS -Wall -Wextra)

add_library(options INTERFACE)
target_compile_features(options INTERFACE c_std_23)
target_compile_options(options INTERFACE ${WARNINGS})
enable_sanitizer(options)
enable_hardening(options)

foreach(q IN ITEMS q1 q2)
    add_executable(${q} src/${q}.c)
    target_link_libraries(${q} PRIVATE options m)
endforeach()

write_compile_flags(LANG C STD gnu23 FLAGS ${WARNINGS})
