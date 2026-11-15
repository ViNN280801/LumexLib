# Consumer fixture for cmake.consumer_expected_mixed_strong_guarantee_*.
#
# A program links one object built with -fno-exceptions (lib.cpp) and one built
# with exceptions (main.cpp); both instantiate the same assignments, emplace_error
# and swap of expected<thrower_t, int>, expected<int, thrower_t> and
# expected<void, thrower_t> (a type whose constructors may throw), so every
# function of the chain has one mangled name in both. They are always inlined and
# the code that puts the old contents back is a guard object, so the linker has
# no weak copy to pick: in the unit with exceptions an operation whose
# constructor throws leaves the old contents intact, whichever object the linker
# sees first, and every object is destroyed once (main.cpp counts constructions
# and destructions). -DMIX_ORDER=LIB_FIRST|MAIN_FIRST sets the link order,
# -DMIX_OPT=O0|O2|O2NI the optimization level (O2NI is -O2 -fno-inline, where
# the compiler inlines nothing by itself). clang-cl runs the three levels (/O2,
# /Od and /O2 /Ob0: it honours always_inline there); cl.exe runs /O2 only:
# without inlining it does not honour __forceinline and keeps one copy.
#
# The operations are reached the way a program reaches them: from functions that
# are not inline (lib_exercise, the scenarios of main.cpp), through the member
# functions and through the free swap, found unqualified by argument-dependent
# lookup. An inline helper or a template between them and the program's
# functions is weak again and outside what the library can promise (see the
# README of expected, "The limit is the caller").

cmake_minimum_required(VERSION 3.16)
project(LumexExpectedMixedStrongGuarantee LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT MIX_ORDER)
    set(MIX_ORDER LIB_FIRST)
endif()
if(NOT MIX_OPT)
    set(MIX_OPT O0)
endif()
set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS FILESYSTEM
        FMT FRAMING PROTOCOL GENERATORS INTERPROCESS REFLECTION STRING
        STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_EXPECTED ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT MIX_OPT STREQUAL "O2")
    # cl.exe does not honour __forceinline without inlining (/Od, /Ob0): there
    # one copy of each function of the chain stays per program and the units
    # must agree on the exception mode. (clang-cl has the compiler id Clang and
    # honours always_inline at every level: it is not skipped.)
    message("LUMEX_CONSUMER_SKIP: cl.exe is checked at /O2 only")
    return()
endif()

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

if(MIX_ORDER STREQUAL "LIB_FIRST")
    set(_sources lib.cpp main.cpp)
else()
    set(_sources main.cpp lib.cpp)
endif()
add_executable(expected_mixed_strong_guarantee_consumer ${_sources})
target_link_libraries(expected_mixed_strong_guarantee_consumer PRIVATE lumex::expected)
set_target_properties(expected_mixed_strong_guarantee_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(MSVC)
    # O0 and O2NI mean /Od and /O2 with inlining off, as for GCC and Clang.
    if(MIX_OPT STREQUAL "O0")
        set(_msvc_opt /Od)
    elseif(MIX_OPT STREQUAL "O2NI")
        set(_msvc_opt /O2 /Ob0)
    else()
        set(_msvc_opt /O2 /Ob2)
    endif()
    target_compile_options(expected_mixed_strong_guarantee_consumer PRIVATE ${_msvc_opt} /Zc:__cplusplus)
elseif(MIX_OPT STREQUAL "O2NI")
    target_compile_options(expected_mixed_strong_guarantee_consumer PRIVATE -O2 -fno-inline)
else()
    target_compile_options(expected_mixed_strong_guarantee_consumer PRIVATE -${MIX_OPT})
endif()
if(MSVC)
    # The unit without exceptions: no /EHsc (CMake puts it first, the later
    # option wins), and the standard library needs _HAS_EXCEPTIONS=0.
    set_source_files_properties(lib.cpp PROPERTIES
        COMPILE_OPTIONS /EHs-c- COMPILE_DEFINITIONS _HAS_EXCEPTIONS=0)
else()
    set_source_files_properties(lib.cpp PROPERTIES COMPILE_OPTIONS -fno-exceptions)
endif()
