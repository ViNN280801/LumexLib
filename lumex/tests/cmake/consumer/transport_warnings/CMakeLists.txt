# Fixture of cmake.consumer_transport_warnings (see main.cpp).
#
# The transport's public headers hold templates (the async_* initiations)
# that the library build never instantiates with a consumer's tokens; a
# consumer that is strict about its code does. One translation unit includes
# every public header and starts each operation with every token style
# (callback, asio::use_future, asio::yield_context, asio::use_awaitable at
# C++20); it is compiled at C++11 and at C++20 with: /W4 /WX (cl.exe),
# -Weverything without the C++98 compatibility groups and /WX (clang-cl and
# Clang), and the warning set of the Lumex targets with -Werror (GCC).

cmake_minimum_required(VERSION 3.16)
project(LumexTransportWarnings LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt HARDWARE JSON LOGGING RESOURCE_MONITOR SETTINGS)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
# With the logger the transport compiles its adapter (log/adapter), whose
# header the translation unit includes and uses too.
set(LUMEX_BUILD_LOGGER ON CACHE BOOL "" FORCE)
set(LUMEX_BUILD_TRANSPORT ON CACHE BOOL "" FORCE)
set(LUMEX_TRANSPORT_WITH_SPAWN ON CACHE BOOL "" FORCE)
set(LUMEX_TRANSPORT_WITH_TESTING ON CACHE BOOL "" FORCE)
# The HID transport where hidapi builds (its option's own default), so that
# its headers and its initiations meet the strict warnings too.

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
    set(_target transport_warnings_cxx${_std})
    add_executable(${_target} main.cpp)
    target_link_libraries(${_target} PRIVATE lumex::transport)
    target_compile_options(${_target} PRIVATE ${_strict})
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_std} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()

# run_consumer.cmake runs <CASE>_consumer: the C++11 build.
add_executable(transport_warnings_consumer main.cpp)
target_link_libraries(transport_warnings_consumer PRIVATE lumex::transport)
target_compile_options(transport_warnings_consumer PRIVATE ${_strict})
set_target_properties(transport_warnings_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
