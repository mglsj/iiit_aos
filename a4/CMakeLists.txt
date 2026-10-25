cmake_minimum_required(VERSION 3.13)

project(mygit CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompilerFlags.cmake)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# On Nix the compiler wrapper injects the OpenSSL/zlib include dirs itself, so
# CMake classes them as "implicit" and drops them from compile_commands.json,
# which leaves clangd unable to find <openssl/sha.h> and <zlib.h>. Re-adding
# them here makes them explicit (-isystem) without changing the real build.
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${OPENSSL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS})

enable_sanitizer()

add_executable(mygit ${SOURCE_FILES})
target_include_directories(mygit PRIVATE src)

target_link_libraries(mygit PRIVATE OpenSSL::Crypto)
target_link_libraries(mygit PRIVATE ZLIB::ZLIB)

set(WARNINGS
  -Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wsign-conversion
  -Wcast-align -Wcast-qual -Wold-style-cast
  -Wnon-virtual-dtor -Woverloaded-virtual
  -Wnull-dereference -Wdouble-promotion
  -Wformat=2 -Wformat-security
  -Wimplicit-fallthrough -Wunused
  -Wmisleading-indentation -Wduplicated-cond
  -Wduplicated-branches -Wlogical-op -Wuseless-cast
  -fstack-protector-strong
  -fno-omit-frame-pointer
)
target_compile_options(mygit PRIVATE ${WARNINGS})
target_compile_definitions(mygit PRIVATE
  _GLIBCXX_ASSERTIONS
  $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
)

if(WARNINGS_AS_ERRORS)
    target_compile_options(mygit PRIVATE -Werror)
endif()

if(SANITIZER MATCHES "undefined")
    target_compile_options(mygit PRIVATE -fno-sanitize-recover=undefined)
endif()

write_compile_flags(
    LANG CXX
    STD c++26
    FLAGS -Isrc ${WARNINGS}
    DEFINES _GLIBCXX_ASSERTIONS
    INCLUDES ${OPENSSL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS}
)
