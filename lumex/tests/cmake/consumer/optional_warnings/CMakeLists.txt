# Fixture of cmake.consumer_optional_warnings (see main.cpp).
#
# optional is header-only, so the library build never compiles it with a strict
# consumer's flags. One translation unit calls every form of value () and the
# other observers, constexpr ones included (the lvalue forms once ended their
# error branch in a comma operand that cl.exe flags as unreachable code, C4702),
# and is compiled at C++11, C++14 and C++20 with: /W4 /WX (cl.exe), -Weverything
# without the C++98 compatibility groups and /WX (clang-cl and Clang), and the
# warning set of the Lumex targets with -Werror (GCC).

cmake_minimum_required(VERSION 3.16)
project(LumexOptionalWarnings LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        BASE64 CIRCULAR_BUFFER CRC ENVIRONMENT EXCEPTIONS EXPECTED FILESYSTEM
        FMT FRAMING PROTOCOL GENERATORS INTERPROCESS REFLECTION STRING
        STRING_VIEW TEMPORARY TIME)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_BUILD_MATH ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_OPTIONAL ON CACHE BOOL "" FORCE)
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
    set(_target optional_warnings_cxx${_std})
    add_executable(${_target} main.cpp)
    target_link_libraries(${_target} PRIVATE lumex::optional)
    target_compile_options(${_target} PRIVATE ${_strict})
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_std} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()

# run_consumer.cmake runs <CASE>_consumer: the C++11 build.
add_executable(optional_warnings_consumer main.cpp)
target_link_libraries(optional_warnings_consumer PRIVATE lumex::optional)
target_compile_options(optional_warnings_consumer PRIVATE ${_strict})
set_target_properties(optional_warnings_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
