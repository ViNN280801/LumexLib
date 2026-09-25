# Compile-check fixture for LumexCMake.format_compile_checks.
#
# LumexFormat checks a literal format string against the argument types at
# compile time from C++20 on (consteval constructor of BasicFormatString).
# Every LUMEX_FORMAT_BAD_CASE in check.cpp must fail to compile and every
# LUMEX_FORMAT_GOOD_CASE must compile. The whole check runs at configure
# time (try_compile); there is nothing to build afterwards. A compiler
# without consteval support prints LUMEX_CONSUMER_SKIP.

cmake_minimum_required(VERSION 3.16)
project(LumexFormatCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
set(_flags "")
if(MSVC)
    # __cplusplus reports 199711L on MSVC without this switch.
    list(APPEND _flags /Zc:__cplusplus /permissive- /EHsc)
endif()

function(_lumex_format_try result_var tag)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${_source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
        COMPILE_DEFINITIONS ${_flags} ${ARGN}
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_VARIABLE _output)
    set(${result_var} ${_ok} PARENT_SCOPE)
    set(${result_var}_OUTPUT "${_output}" PARENT_SCOPE)
endfunction()

_lumex_format_try(_probe probe)
if(NOT _probe)
    message(STATUS "LUMEX_CONSUMER_SKIP: the compiler has no C++20 consteval")
    return()
endif()

set(_good_cases 7)
set(_bad_cases 12)

foreach(_case RANGE 1 ${_good_cases})
    _lumex_format_try(_ok good_${_case} -DLUMEX_FORMAT_GOOD_CASE=${_case})
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_FORMAT_GOOD_CASE=${_case} must compile\n${_ok_OUTPUT}")
    endif()
endforeach()

foreach(_case RANGE 1 ${_bad_cases})
    _lumex_format_try(_ok bad_${_case} -DLUMEX_FORMAT_BAD_CASE=${_case})
    if(_ok)
        message(FATAL_ERROR
            "LUMEX_FORMAT_BAD_CASE=${_case} compiled; the consteval format "
            "string check did not reject it")
    endif()
endforeach()

message(STATUS "format compile checks: ${_good_cases} good, "
               "${_bad_cases} bad cases as expected")
