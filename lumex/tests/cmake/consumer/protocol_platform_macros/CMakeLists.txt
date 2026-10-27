# Fixture of cmake.consumer_protocol_platform_macros (see main.cpp).
#
# The collision guard of the protocol tables: a table whose specification
# names are platform macro names compiles after <windows.h> (Windows) or the
# usual POSIX headers (elsewhere), with X_AS rows, its layer, its entries and
# the client methods named by the bare row identifiers. Built at C++11 and at
# C++20; the C++11 build runs. The platform headers are not compiled with a
# strict warning set (cmake.consumer_protocol_warnings is the strict one).

cmake_minimum_required(VERSION 3.16)
project(LumexProtocolPlatformMacros LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        ATOMIC BASE64 CIRCULAR_BUFFER ENVIRONMENT EXCEPTIONS FILESYSTEM FMT
        GENERATORS HAZARD_POINTER INTERPROCESS SMART_PTR STRING TEMPORARY TIME
        UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt
        CONTRACTS CRC EXPECTED FRAMING MATH OPTIONAL PROTOCOL REFLECTION SPAN
        STRING_VIEW UTILITY)
    set(LUMEX_BUILD_${_opt} ON CACHE BOOL "" FORCE)
endforeach()

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

foreach(_std 11 20)
    set(_target protocol_platform_macros_cxx${_std})
    add_executable(${_target} main.cpp)
    target_link_libraries(${_target} PRIVATE lumex::protocol)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_std} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endforeach()

# run_consumer.cmake runs <CASE>_consumer: the C++11 build.
add_executable(protocol_platform_macros_consumer main.cpp)
target_link_libraries(protocol_platform_macros_consumer PRIVATE
    lumex::protocol)
set_target_properties(protocol_platform_macros_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
