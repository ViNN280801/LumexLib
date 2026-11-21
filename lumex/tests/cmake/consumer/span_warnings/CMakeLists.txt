# Fixture of cmake.consumer_span_warnings (see main.cpp).
#
# span is header-only, so the library build never compiles it with a strict
# consumer's flags. One translation unit calls every element accessor of span,
# at () of a const span included (it once ended its error branch in a comma
# operand that cl.exe flags as unreachable code, C4702), static and dynamic
# extents, constexpr ones included, and is compiled at C++11 (cl.exe and
# clang-cl have no flag below C++14), C++14 and C++20 with: /W4 /WX (cl.exe),
# -Weverything without the C++98 compatibility groups and /WX (clang-cl and
# Clang), and the warning set of the Lumex targets with -Werror (GCC).

cmake_minimum_required(VERSION 3.16)
project(LumexSpanWarnings LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT FRAMING PROTOCOL UNITS GENERATORS INTERPROCESS REFLECTION STRING
        STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_OPTIONAL ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_SPAN ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_UTILITY ON CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(_strict /W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(_strict -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic
        -Wno-c++98-compat-local-type-template-args -Wno-pre-c++14-compat
        -Wno-pre-c++17-compat -Wno-padded)
    if(MSVC)
        list(APPEND _strict /WX)
    else()
        list(APPEND _strict -Werror)
    endif()
else()
    set(_strict -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
        -Wshadow -Wold-style-cast -Wcast-qual -Wundef -Wswitch-default
        -Wswitch-enum -Wnon-virtual-dtor -Woverloaded-virtual -Werror)
endif()

foreach(_std 11 14 20)
    set(_target span_warnings_cxx${_std})
    add_executable(${_target} main.cpp)
    target_link_libraries(${_target} PRIVATE lumex::span)
    target_compile_options(${_target} PRIVATE ${_strict})
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_std} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()

# run_consumer.cmake runs <CASE>_consumer: the C++11 build.
add_executable(span_warnings_consumer main.cpp)
target_link_libraries(span_warnings_consumer PRIVATE lumex::span)
target_compile_options(span_warnings_consumer PRIVATE ${_strict})
set_target_properties(span_warnings_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
