# Compile-check fixture for cmake.span_compile_checks.
#
# Every LUMEX_SPAN_BAD_CASE in check.cpp must fail to compile with warnings as
# errors, and LUMEX_SPAN_GOOD_CASE must compile. The cases are the uses
# std::span rejects: a discarded result of an accessor (nodiscard, cases
# 1-4), a constructor the constraints remove (5-9, 14-17, 19, 20, 25, 26, 28-34),
# a static_assert of a member (10-13), a missing function (18, 27), an
# assignment to a const element (21) and invalid element types (22-24: void,
# a reference, a function).
# The checks run at C++11, C++17 and C++20. Everything runs at configure time
# (try_compile).

cmake_minimum_required(VERSION 3.16)
project(LumexSpanCompileChecks LANGUAGES CXX)

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
set(_last_case 34)

# The files are compiled, not run: a static library needs no handler.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

function(_lumex_span_try_file source result_var tag standard)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
        COMPILE_DEFINITIONS ${_werror} ${ARGN}
        CXX_STANDARD ${standard}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_VARIABLE _output)
    set(${result_var} ${_ok} PARENT_SCOPE)
    set(${result_var}_OUTPUT "${_output}" PARENT_SCOPE)
endfunction()

# check.cpp does not exercise the checks: they are off there.
function(_lumex_span_try result_var tag standard)
    _lumex_span_try_file("${_source}" ${result_var} ${tag} ${standard}
                         -DLUMEX_SPAN_CHECKS=0 ${ARGN})
    set(${result_var} ${${result_var}} PARENT_SCOPE)
    set(${result_var}_OUTPUT "${${result_var}_OUTPUT}" PARENT_SCOPE)
endfunction()

set(_checked 0)
foreach(_std IN LISTS _standards)
    set(_tag "cxx${_std}")
    _lumex_span_try(_ok good_${_tag} ${_std} -DLUMEX_SPAN_GOOD_CASE)
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_SPAN_GOOD_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
    endif()

    foreach(_case RANGE 1 ${_last_case})
        # Cases 1-4 discard a [[nodiscard]] result. Only a compiler with the
        # attribute warns about that, and cl.exe has it from C++17 alone
        # (LUMEX_NODISCARD is empty there at C++14), so at C++14 the check
        # cannot reject them: the case is not one cl.exe can fail. The test is
        # the compiler id, not the MSVC variable - that one is also true for
        # clang-cl, which does have the attribute at C++14.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND _std LESS 17 AND _case LESS 5)
            continue()
        endif()
        _lumex_span_try(_ok bad_${_case}_${_tag} ${_std}
                        -DLUMEX_SPAN_BAD_CASE=${_case})
        if(_ok)
            message(FATAL_ERROR
                "LUMEX_SPAN_BAD_CASE=${_case} (${_tag}) compiled; the "
                "check that should reject it is missing")
        endif()
        # A case must fail for its own reason, not because check.cpp has a
        # typo: the error text must not be the #error of an unknown case or a
        # missing name (GCC, Clang and MSVC spell the second one differently).
        if(_ok_OUTPUT MATCHES "is not one of the cases")
            message(FATAL_ERROR
                "LUMEX_SPAN_BAD_CASE=${_case} is not defined in check.cpp")
        endif()
        if(_ok_OUTPUT MATCHES "not declared in this scope|undeclared identifier|use of undeclared|C2065")
            message(FATAL_ERROR
                "LUMEX_SPAN_BAD_CASE=${_case} (${_tag}) fails on a missing "
                "name, not on the check\n${_ok_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "span compile checks: ${_checked} rejected cases as expected")

# -- The checks in constant expressions (check_constexpr.cpp) --
# With the checks on a violated precondition is not a constant expression;
# with them off the same code compiles, so the check is the only difference.
set(_constexpr_source "${CMAKE_CURRENT_SOURCE_DIR}/check_constexpr.cpp")
set(_last_constexpr_case 14)
set(_constexpr_checked 0)
foreach(_std IN LISTS _standards)
    set(_tag "cxx${_std}")
    foreach(_checks 1 0)
        _lumex_span_try_file("${_constexpr_source}" _ok good_c${_checks}_${_tag}
            ${_std} -DLUMEX_SPAN_GOOD_CASE -DLUMEX_SPAN_CHECKS=${_checks})
        if(NOT _ok)
            message(FATAL_ERROR
                "check_constexpr LUMEX_SPAN_GOOD_CASE (${_tag}, checks "
                "${_checks}) must compile\n${_ok_OUTPUT}")
        endif()
    endforeach()
    foreach(_case RANGE 1 ${_last_constexpr_case})
        _lumex_span_try_file("${_constexpr_source}" _on bad_c1_${_case}_${_tag}
            ${_std} -DLUMEX_SPAN_BAD_CASE=${_case} -DLUMEX_SPAN_CHECKS=1)
        if(_on)
            message(FATAL_ERROR
                "check_constexpr case ${_case} (${_tag}) compiled with the "
                "checks on: a violated precondition is a constant expression")
        endif()
        if(_on_OUTPUT MATCHES "is not one of the cases"
           OR _on_OUTPUT MATCHES "not declared in this scope|undeclared identifier|use of undeclared|C2065")
            message(FATAL_ERROR
                "check_constexpr case ${_case} (${_tag}) fails for another "
                "reason\n${_on_OUTPUT}")
        endif()
        # The off form of a case whose precondition is violated reads out of
        # the array: GCC and Clang of the other platforms fold it by accident
        # (the read lands inside the object), while clang-cl refuses the
        # constant evaluation ("constexpr variable 'result' must be
        # initialized by a constant expression"). Only the checked side is
        # compared there.
        if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang"
                AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
            _lumex_span_try_file("${_constexpr_source}" _off bad_c0_${_case}_${_tag}
                ${_std} -DLUMEX_SPAN_BAD_CASE=${_case} -DLUMEX_SPAN_CHECKS=0)
            if(NOT _off)
                message(FATAL_ERROR
                    "check_constexpr case ${_case} (${_tag}) must compile with "
                    "the checks off, or the check is not what rejects it\n"
                    "${_off_OUTPUT}")
            endif()
        endif()
        math(EXPR _constexpr_checked "${_constexpr_checked} + 1")
    endforeach()
endforeach()
message(STATUS "span constexpr checks: ${_constexpr_checked} violated "
    "preconditions rejected with the checks on, accepted with them off")

# -- The value of LUMEX_SPAN_CHECKS (check_config.cpp) --
set(_config_source "${CMAKE_CURRENT_SOURCE_DIR}/check_config.cpp")
function(_lumex_span_config tag expect should_compile)
    _lumex_span_try_file("${_config_source}" _ok config_${tag} 11
        -DLUMEX_SPAN_EXPECT=${expect} ${ARGN})
    if(should_compile AND NOT _ok)
        message(FATAL_ERROR "config ${tag}: expected ${expect}\n${_ok_OUTPUT}")
    endif()
    if(NOT should_compile AND _ok)
        message(FATAL_ERROR "config ${tag}: the check accepted a wrong value")
    endif()
endfunction()
if(MSVC)
    set(_ndebug_on /DNDEBUG)
    set(_ndebug_off /UNDEBUG)
else()
    set(_ndebug_on -DNDEBUG)
    set(_ndebug_off -UNDEBUG)
endif()
# default: on without NDEBUG, off with it
_lumex_span_config(default_debug 1 TRUE ${_ndebug_off})
_lumex_span_config(default_release 0 TRUE ${_ndebug_on})
_lumex_span_config(wrong_debug 0 FALSE ${_ndebug_off})
_lumex_span_config(wrong_release 1 FALSE ${_ndebug_on})
# the program decides
_lumex_span_config(forced_on 1 TRUE ${_ndebug_on} -DLUMEX_SPAN_CHECKS=1)
_lumex_span_config(forced_off 0 TRUE ${_ndebug_off} -DLUMEX_SPAN_CHECKS=0)
_lumex_span_config(forced_on_not_off 0 FALSE ${_ndebug_on}
    -DLUMEX_SPAN_CHECKS=1)

# -- MSVC link-time mismatch of the macro (detect_mismatch) --
# Preprocessed as if by MSVC: the pragma names the macro and its value.
if(NOT MSVC)
    foreach(_value 0 1)
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" -E -P -D_MSC_VER=1930
                    -DLUMEX_SPAN_CHECKS=${_value}
                    -I "${LUMEX_SOURCE_DIR}"
                    "${LUMEX_SOURCE_DIR}/lumex/core/span/view/LumexSpanChecks.hpp"
            OUTPUT_VARIABLE _pp ERROR_VARIABLE _pp_err RESULT_VARIABLE _pp_rc)
        if(NOT _pp_rc EQUAL 0)
            message(FATAL_ERROR "preprocessing LumexSpanChecks.hpp: ${_pp_err}")
        endif()
        string(FIND "${_pp}"
            "#pragma detect_mismatch(\"LUMEX_SPAN_CHECKS\", \"${_value}\")" _at)
        if(_at EQUAL -1)
            message(FATAL_ERROR
                "LumexSpanChecks.hpp does not record LUMEX_SPAN_CHECKS=${_value} "
                "with detect_mismatch under _MSC_VER")
        endif()
    endforeach()
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -E -P -DLUMEX_SPAN_CHECKS=1
                -I "${LUMEX_SOURCE_DIR}"
                "${LUMEX_SOURCE_DIR}/lumex/core/span/view/LumexSpanChecks.hpp"
        OUTPUT_VARIABLE _pp)
    string(FIND "${_pp}" "detect_mismatch" _at)
    if(NOT _at EQUAL -1)
        message(FATAL_ERROR "detect_mismatch must be MSVC-only")
    endif()
endif()
