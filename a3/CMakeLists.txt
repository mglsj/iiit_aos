cmake_minimum_required(VERSION 3.16)
project(tor CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompilerFlags.cmake)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(WARNINGS -freflection -Wall -Wextra -Wpedantic -Wshadow -Wconversion
             -Wsign-conversion -fstack-protector-strong)

add_compile_options(${WARNINGS})
add_compile_definitions(_GLIBCXX_ASSERTIONS)
enable_sanitizer()

# glibc warns about _FORTIFY_SOURCE without optimisation
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(_FORTIFY_SOURCE=3)
endif()

find_package(Threads REQUIRED)

file(GLOB COMMON_SRC CONFIGURE_DEPENDS common/*.cpp)
file(GLOB TRACKER_SRC CONFIGURE_DEPENDS tracker/*.cpp)
file(GLOB CLIENT_SRC CONFIGURE_DEPENDS client/*.cpp)

add_library(common STATIC ${COMMON_SRC})
target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR})
# stdc++exp holds the handler that reports a failed contract
target_link_libraries(common PUBLIC Threads::Threads stdc++exp)

add_executable(tracker ${TRACKER_SRC})
target_link_libraries(tracker PRIVATE common)

add_executable(client ${CLIENT_SRC})
target_link_libraries(client PRIVATE common)

write_compile_flags(
    LANG CXX
    STD c++26
    FLAGS -I. ${WARNINGS}
    DEFINES _GLIBCXX_ASSERTIONS
)
