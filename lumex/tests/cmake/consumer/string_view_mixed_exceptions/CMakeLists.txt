# Consumer fixture for cmake.consumer_string_view_mixed_exceptions_*.
#
# A program links one object built with -fno-exceptions (lib.cpp) and one built
# with exceptions (main.cpp), both include the string view header, both use
# at () and operator<<. The members whose body depends on the exception mode
# are always inlined, so the linker has no weak copy to pick: the exception
# enabled unit throws std::out_of_range and its operator<< sets the badbit
# whichever object the linker sees first. -DMIX_ORDER=LIB_FIRST|MAIN_FIRST
# sets the link order, -O0 keeps the compiler from inlining by itself.

cmake_minimum_required(VERSION 3.16)
project(LumexStringViewMixedExceptions LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()
if(NOT MIX_ORDER)
    set(MIX_ORDER LIB_FIRST)
endif()
if(MSVC)
    message("LUMEX_CONSUMER_SKIP: MSVC has no -fno-exceptions")
    return()
endif()

if(MIX_ORDER STREQUAL "LIB_FIRST")
    set(_sources lib.cpp main.cpp)
else()
    set(_sources main.cpp lib.cpp)
endif()
add_executable(string_view_mixed_exceptions_consumer ${_sources})
target_include_directories(string_view_mixed_exceptions_consumer PRIVATE "${LUMEX_SOURCE_DIR}")
set_target_properties(string_view_mixed_exceptions_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(string_view_mixed_exceptions_consumer PRIVATE -O0)
set_source_files_properties(lib.cpp PROPERTIES COMPILE_OPTIONS -fno-exceptions)
