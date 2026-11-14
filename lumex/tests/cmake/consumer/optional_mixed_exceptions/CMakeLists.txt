# Consumer fixture for cmake.consumer_optional_mixed_exceptions_*.
#
# A program links one object built with -fno-exceptions (lib.cpp) and one built
# with exceptions (main.cpp); both include the optional header and call value ()
# of the same optional<int>, so the four overloads have one mangled name in
# both. value () and what it calls are always inlined, so the linker has no
# weak copy to pick: the exception enabled unit throws
# lumex_bad_optional_access whichever object the linker sees first, and the
# unit without exceptions terminates. -DMIX_ORDER=LIB_FIRST|MAIN_FIRST sets the
# link order, -DMIX_OPT=O0|O2|O2NI the optimization level (O2NI is -O2
# -fno-inline: GCC at -O2 inlines value () by itself and then shows no defect,
# the compiler that does not inline is the one that needs always_inline).

cmake_minimum_required(VERSION 3.16)
project(LumexOptionalMixedExceptions LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT MIX_ORDER)
    set(MIX_ORDER LIB_FIRST)
endif()
if(NOT MIX_OPT)
    set(MIX_OPT O0)
endif()
if(MSVC)
    message("LUMEX_CONSUMER_SKIP: one object without exceptions and one with them is a GCC and Clang link")
    return()
endif()

if(MIX_ORDER STREQUAL "LIB_FIRST")
    set(_sources lib.cpp main.cpp)
else()
    set(_sources main.cpp lib.cpp)
endif()
add_executable(optional_mixed_exceptions_consumer ${_sources})
target_include_directories(optional_mixed_exceptions_consumer PRIVATE "${LUMEX_SOURCE_DIR}")
set_target_properties(optional_mixed_exceptions_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(MIX_OPT STREQUAL "O2NI")
    target_compile_options(optional_mixed_exceptions_consumer PRIVATE -O2 -fno-inline)
else()
    target_compile_options(optional_mixed_exceptions_consumer PRIVATE -${MIX_OPT})
endif()
set_source_files_properties(lib.cpp PROPERTIES COMPILE_OPTIONS -fno-exceptions)
