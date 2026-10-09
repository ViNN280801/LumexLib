# Compile-check fixture for cmake.math_constants_compile_checks.
#
# Every LUMEX_MATH_CONSTANTS_BAD_CASE in check.cpp must fail to compile with
# warnings as errors, and LUMEX_MATH_CONSTANTS_GOOD_CASE must compile. The
# constants are macros that expand to double literals, so the cases are the
# uses a literal rejects (no address, no non-const reference, no assignment,
# no modulo, no narrowing to an integer in braces, no value in an #if), a
# suffix that is not a floating one, and a float literal that is not the same
# number as the double macro (the precisions are different numbers).
# The checks run at C++11, C++17 and C++20. Everything runs at configure time
# (try_compile).

cmake_minimum_required(VERSION 3.16)
project(LumexMathConstantsCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

# The compiler's messages are matched in English below.
set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
if(MSVC)
    set(_werror /WX /Zc:__cplusplus /permissive- /EHsc)
    set(_standards 14 17 20)
else()
    set(_werror -Werror)
    set(_standards 11 17 20)
endif()
set(_last_case 9)
if(MSVC)
    # long double is a double there: case 9 compares two equal numbers.
    set(_last_case 8)
endif()

function(_lumex_math_constants_try result_var tag standard)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${_source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
        COMPILE_DEFINITIONS ${_werror} ${ARGN}
        CXX_STANDARD ${standard}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_VARIABLE _output)
    set(${result_var} ${_ok} PARENT_SCOPE)
    set(${result_var}_OUTPUT "${_output}" PARENT_SCOPE)
endfunction()

set(_checked 0)
foreach(_std IN LISTS _standards)
    set(_tag "cxx${_std}")
    _lumex_math_constants_try(_ok good_${_tag} ${_std}
                              -DLUMEX_MATH_CONSTANTS_GOOD_CASE)
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_MATH_CONSTANTS_GOOD_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
    endif()

    foreach(_case RANGE 1 ${_last_case})
        _lumex_math_constants_try(_ok bad_${_case}_${_tag} ${_std}
                                  -DLUMEX_MATH_CONSTANTS_BAD_CASE=${_case})
        if(_ok)
            message(FATAL_ERROR
                "LUMEX_MATH_CONSTANTS_BAD_CASE=${_case} (${_tag}) compiled; "
                "the check that should reject it is missing")
        endif()
        # A case must fail for its own reason, not because check.cpp has a
        # typo: the error text must not be the #error of an unknown case or a
        # missing name (GCC, Clang and MSVC spell the second one differently).
        if(_ok_OUTPUT MATCHES "is not one of the cases")
            message(FATAL_ERROR
                "LUMEX_MATH_CONSTANTS_BAD_CASE=${_case} is not defined in "
                "check.cpp")
        endif()
        if(_ok_OUTPUT MATCHES "not declared in this scope|undeclared identifier|use of undeclared|C2065")
            message(FATAL_ERROR
                "LUMEX_MATH_CONSTANTS_BAD_CASE=${_case} (${_tag}) fails on a "
                "missing name, not on the check\n${_ok_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "math constants compile checks: ${_checked} rejected cases as expected")
