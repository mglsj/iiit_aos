cmake_minimum_required(VERSION 3.16)
project(tor CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-freflection -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                    -Wsign-conversion -fstack-protector-strong)
add_compile_definitions(_GLIBCXX_ASSERTIONS)

option(ENABLE_ASAN "AddressSanitizer + UBSan" OFF)
option(ENABLE_TSAN "ThreadSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -g)
    add_link_options(-fsanitize=thread)
endif()

# glibc warns about _FORTIFY_SOURCE without optimisation, and the sanitizers
# do their own checks of the same calls
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ENABLE_ASAN AND NOT ENABLE_TSAN)
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
