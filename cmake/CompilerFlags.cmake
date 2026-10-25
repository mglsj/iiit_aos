# Shared by every project in the monorepo.
#
#   SANITIZER   "" (default), "address", "address,undefined" or "thread"
#
#   enable_sanitizer()          applies -DSANITIZER to every target in the directory
#   write_compile_flags(...)    writes <project>/compile_flags.txt for clangd

set(SANITIZER "" CACHE STRING "Sanitizers to build with (address, address,undefined, thread)")

function(enable_sanitizer)
    if(NOT SANITIZER)
        return()
    endif()
    if(SANITIZER MATCHES "thread" AND SANITIZER MATCHES "address")
        message(FATAL_ERROR "ThreadSanitizer cannot be combined with AddressSanitizer")
    endif()
    add_compile_options(-fsanitize=${SANITIZER} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${SANITIZER})
endfunction()

# The Nix compiler wrapper injects its include paths through the environment, so
# they never reach compile_commands.json and clangd cannot find the standard
# headers. Reading them back from CMake's compiler probe makes them explicit.
# Sanitizer flags are left out so the file is the same whichever build wrote it.
#
#   write_compile_flags(LANG C|CXX STD <std> [FLAGS ...] [DEFINES ...] [INCLUDES ...])
function(write_compile_flags)
    cmake_parse_arguments(A "" "LANG;STD" "FLAGS;DEFINES;INCLUDES" ${ARGN})

    set(lines "-std=${A_STD}" ${A_FLAGS})
    foreach(def IN LISTS A_DEFINES)
        list(APPEND lines "-D${def}")
    endforeach()
    foreach(dir IN LISTS CMAKE_${A_LANG}_IMPLICIT_INCLUDE_DIRECTORIES A_INCLUDES)
        list(APPEND lines "-isystem" "${dir}")
    endforeach()

    list(JOIN lines "\n" body)
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/compile_flags.txt" "${body}\n")
endfunction()
