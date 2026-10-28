cmake_minimum_required(VERSION 4.4)

project(tor LANGUAGES CXX)

include(${PROJECT_SOURCE_DIR}/../cmake/CompilerFlags.cmake)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(WARNINGS -freflection -Wall -Wextra -Wpedantic -Wshadow -Wconversion
             -Wsign-conversion -fstack-protector-strong)

add_library(options INTERFACE)
target_compile_features(options INTERFACE cxx_std_26)
target_compile_options(options INTERFACE ${WARNINGS})
target_compile_definitions(options INTERFACE
    _GLIBCXX_ASSERTIONS
    # glibc warns about _FORTIFY_SOURCE without optimisation
    $<$<CONFIG:Release>:_FORTIFY_SOURCE=3>
)
enable_sanitizer(options)

find_package(Threads REQUIRED)

file(GLOB COMMON_SRC CONFIGURE_DEPENDS common/*.cpp)
file(GLOB TRACKER_SRC CONFIGURE_DEPENDS tracker/*.cpp)
file(GLOB CLIENT_SRC CONFIGURE_DEPENDS client/*.cpp)

add_library(common STATIC)
target_sources(common PRIVATE ${COMMON_SRC})
target_include_directories(common PUBLIC ${PROJECT_SOURCE_DIR})
# stdc++exp holds the handler that reports a failed contract
target_link_libraries(common PUBLIC options Threads::Threads stdc++exp)

add_executable(tracker)
target_sources(tracker PRIVATE ${TRACKER_SRC})
target_link_libraries(tracker PRIVATE common)

add_executable(client)
target_sources(client PRIVATE ${CLIENT_SRC})
target_link_libraries(client PRIVATE common)

write_compile_flags(
    LANG CXX
    STD c++26
    FLAGS -I. ${WARNINGS}
    DEFINES _GLIBCXX_ASSERTIONS
)
