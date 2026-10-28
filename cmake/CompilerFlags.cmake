# Shared by every project in the monorepo. Each function takes an INTERFACE
# library; link it to a target to give that target (and its dependents) the flags.
#
#   SANITIZER   "" (default), "address", "address,undefined" or "thread"
#
#   enable_sanitizer(<lib>)     -fsanitize=${SANITIZER}
#   enable_hardening(<lib>)     stack, format and link-time protections
#   write_compile_flags(...)    writes <project>/compile_flags.txt for clangd

set(SANITIZER "" CACHE STRING "Sanitizers to build with (address, address,undefined, thread)")

function(enable_sanitizer lib)
    if(NOT SANITIZER)
        return()
    endif()
    if(SANITIZER MATCHES "thread" AND SANITIZER MATCHES "address")
        message(FATAL_ERROR "ThreadSanitizer cannot be combined with AddressSanitizer")
    endif()
    target_compile_options(${lib} INTERFACE -fsanitize=${SANITIZER} -fno-omit-frame-pointer -g)
    target_link_options(${lib} INTERFACE -fsanitize=${SANITIZER})
endfunction()

function(enable_hardening lib)
    target_compile_options(${lib} INTERFACE
        -fstack-protector-strong
        -fstack-clash-protection
        -Wformat=2 -Wformat-security -Werror=format-security
        $<$<PLATFORM_ID:Linux>:$<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>:-fcf-protection=full>>
    )
    # glibc warns about _FORTIFY_SOURCE without optimisation, and the sanitizers
    # do their own checks of the same calls
    target_compile_definitions(${lib} INTERFACE $<$<CONFIG:Release>:_FORTIFY_SOURCE=3>)
    target_link_options(${lib} INTERFACE LINKER:-z,relro,-z,now)
endfunction()

# The Nix compiler wrapper injects its include paths through the environment, so
# they never reach compile_commands.json and clangd cannot find the standard
# headers. Reading them back from CMake's compiler probe makes them explicit.
# Sanitizer flags are left out so the file is the same whichever build wrote it.
#
#   write_compile_flags(LANG C|CXX STD <std> [FLAGS ...] [DEFINES ...] [INCLUDES ...])
function(write_compile_flags)
    cmake_parse_arguments(PARSE_ARGV 0 A "" "LANG;STD" "FLAGS;DEFINES;INCLUDES")

    set(lines "-std=${A_STD}" ${A_FLAGS})
    foreach(def IN LISTS A_DEFINES)
        list(APPEND lines "-D${def}")
    endforeach()
    foreach(dir IN LISTS CMAKE_${A_LANG}_IMPLICIT_INCLUDE_DIRECTORIES A_INCLUDES)
        list(APPEND lines "-isystem" "${dir}")
    endforeach()

    list(JOIN lines "\n" body)
    file(WRITE "${PROJECT_SOURCE_DIR}/compile_flags.txt" "${body}\n")
endfunction()
