# Fixture of cmake.consumer_framing_warnings (see main.cpp).
#
# The framing codecs are header-only, so the library build never compiles them
# with its own warning flags; a consumer that is strict about its code does.
# One translation unit instantiates every codec, stage and pipeline with both
# sink forms and is compiled at C++11 and at C++20 with: /W4 /WX (cl.exe),
# -Weverything without the C++98 compatibility groups and /WX (clang-cl and
# Clang), and the warning set of the Lumex targets with -Werror (GCC).

cmake_minimum_required(VERSION 3.16)
project(LumexFramingWarnings LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        ATOMIC BASE64 CIRCULAR_BUFFER ENVIRONMENT EXCEPTIONS FILESYSTEM FMT
        GENERATORS HAZARD_POINTER PROTOCOL INTERPROCESS REFLECTION SMART_PTR STRING TEMPORARY TIME
        UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt
        CONTRACTS CRC EXPECTED FRAMING MATH OPTIONAL SPAN STRING_VIEW UTILITY)
    set(LUMEX_BUILD_${_opt} ON CACHE BOOL "" FORCE)
endforeach()

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

foreach(_std 11 20)
    set(_target framing_warnings_cxx${_std})
    add_executable(${_target} main.cpp)
    target_link_libraries(${_target} PRIVATE lumex::framing)
    target_compile_options(${_target} PRIVATE ${_strict})
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_std} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()

# run_consumer.cmake runs <CASE>_consumer: the C++11 build.
add_executable(framing_warnings_consumer main.cpp)
target_link_libraries(framing_warnings_consumer PRIVATE lumex::framing)
target_compile_options(framing_warnings_consumer PRIVATE ${_strict})
set_target_properties(framing_warnings_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
