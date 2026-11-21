# Consumer fixture for cmake.consumer_expected_mixed_exceptions_*.
#
# A program links one object built with -fno-exceptions (lib.cpp) and one built
# with exceptions (main.cpp); both include the expected header and call value ()
# of the same expected<int, int>, so the six overloads have one mangled name in
# both. value () and what it calls are always inlined, so the linker has no
# weak copy to pick: the exception enabled unit throws
# bad_expected_access whichever object the linker sees first, and the
# unit without exceptions terminates. -DMIX_ORDER=LIB_FIRST|MAIN_FIRST sets the
# link order, -DMIX_OPT=O0|O2|O2NI the optimization level (O2NI is -O2
# -fno-inline: GCC at -O2 inlines value () by itself and then shows no defect,
# the compiler that does not inline is the one that needs always_inline).
# Only value () is covered here; the operations that put the old contents back
# are covered by the expected_mixed_strong_guarantee fixture.

cmake_minimum_required(VERSION 3.16)
project(LumexExpectedMixedExceptions LANGUAGES CXX)

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
        FMT FRAMING PROTOCOL UNITS GENERATORS INTERPROCESS REFLECTION STRING
        STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_EXPECTED ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

if(MSVC AND NOT MIX_OPT STREQUAL "O2")
    # cl.exe does not honour __forceinline without inlining (/Od, /Ob0): there
    # one copy of value () and at () stays per program and the units must agree
    # on the exception mode. -O0 and -O2 -fno-inline have no meaning for cl.exe.
    message("LUMEX_CONSUMER_SKIP: cl.exe and clang-cl are checked at /O2 only")
    return()
endif()

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

if(MIX_ORDER STREQUAL "LIB_FIRST")
    set(_sources lib.cpp main.cpp)
else()
    set(_sources main.cpp lib.cpp)
endif()
add_executable(expected_mixed_exceptions_consumer ${_sources})
target_link_libraries(expected_mixed_exceptions_consumer PRIVATE lumex::expected)
set_target_properties(expected_mixed_exceptions_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(expected_mixed_exceptions_consumer PRIVATE /O2 /Ob2 /Zc:__cplusplus)
elseif(MIX_OPT STREQUAL "O2NI")
    target_compile_options(expected_mixed_exceptions_consumer PRIVATE -O2 -fno-inline)
else()
    target_compile_options(expected_mixed_exceptions_consumer PRIVATE -${MIX_OPT})
endif()
if(MSVC)
    # The unit without exceptions: no /EHsc (CMake puts it first, the later
    # option wins), and the standard library needs _HAS_EXCEPTIONS=0.
    set_source_files_properties(lib.cpp PROPERTIES
        COMPILE_OPTIONS /EHs-c- COMPILE_DEFINITIONS _HAS_EXCEPTIONS=0)
else()
    set_source_files_properties(lib.cpp PROPERTIES COMPILE_OPTIONS -fno-exceptions)
endif()
