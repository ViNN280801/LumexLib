# Fixture of cmake.consumer_protocol_without_json (see main.cpp).
#
# LUMEX_PROTOCOL_WITH_JSON is OFF: the module builds and works without JSON
# (the registry, the index, describe and the Markdown export), the macro is not
# defined for the consumer, and the JSON header refuses to compile with its own
# message instead of failing on a missing nlohmann include. With the macro and
# the vendored nlohmann/json on the include path the same header compiles, so
# the refusal is the macro and not a typo.

cmake_minimum_required(VERSION 3.16)
project(LumexProtocolWithoutJson LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(LUMEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_APPLIED OFF CACHE BOOL "" FORCE)
set(LUMEX_BUILD_XML OFF CACHE BOOL "" FORCE)
foreach(_opt
        ATOMIC BASE64 CIRCULAR_BUFFER ENVIRONMENT EXCEPTIONS FILESYSTEM FMT
        GENERATORS HAZARD_POINTER SMART_PTR STRING TEMPORARY TIME UNICODE)
    set(LUMEX_BUILD_${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(_opt
        CONTRACTS CRC EXPECTED FRAMING MATH OPTIONAL PROTOCOL REFLECTION SPAN
        STRING_VIEW UTILITY)
    set(LUMEX_BUILD_${_opt} ON CACHE BOOL "" FORCE)
endforeach()
set(LUMEX_PROTOCOL_WITH_JSON OFF CACHE BOOL "" FORCE)

add_subdirectory("${LUMEX_SOURCE_DIR}" lumexlib)

# run_consumer.cmake runs <CASE>_consumer.
add_executable(protocol_without_json_consumer main.cpp)
target_link_libraries(protocol_without_json_consumer PRIVATE lumex::protocol)
set_target_properties(protocol_without_json_consumer PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

# The JSON header on its own, at configure time (try_compile).
set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)
set(_source "${CMAKE_CURRENT_SOURCE_DIR}/json_header.cpp")
set(_includes "${LUMEX_SOURCE_DIR};${LUMEX_SOURCE_DIR}/3rdparty")
if(MSVC)
    set(_flags /Zc:__cplusplus /permissive- /EHsc)
else()
    set(_flags "")
endif()

try_compile(_without "${CMAKE_BINARY_DIR}/try_without"
    SOURCES "${_source}"
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${_includes}"
    COMPILE_DEFINITIONS ${_flags}
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_VARIABLE _without_output)
if(_without)
    message(FATAL_ERROR
        "LumexProtocolExportJson.hpp compiled without LUMEX_PROTOCOL_WITH_JSON")
endif()
if(NOT _without_output MATCHES "needs LUMEX_PROTOCOL_WITH_JSON")
    message(FATAL_ERROR
        "the JSON header failed, but not with its own message\n"
        "${_without_output}")
endif()

try_compile(_with "${CMAKE_BINARY_DIR}/try_with"
    SOURCES "${_source}"
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${_includes}"
    COMPILE_DEFINITIONS ${_flags} -DLUMEX_PROTOCOL_WITH_JSON=1
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_VARIABLE _with_output)
if(NOT _with)
    message(FATAL_ERROR
        "LumexProtocolExportJson.hpp must compile with the macro\n"
        "${_with_output}")
endif()
