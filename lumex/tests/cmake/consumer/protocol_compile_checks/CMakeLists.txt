# Compile-check fixture for cmake.protocol_compile_checks.
#
# The protocol table DSL (lumex/core/protocol/dsl) must refuse a table it
# cannot generate with the message of its own check. Every
# LUMEX_PROTOCOL_BAD_CASE in check.cpp must fail to compile with that
# message, and LUMEX_PROTOCOL_GOOD_CASE (the same rows, valid) must compile:
#   1 a row with fewer field names than fields
#   2 a row with more field names than fields
#   3 a layout with fewer names than fields
#   4 a row identifier that is a platform macro name (DELETE) without X_AS
#   5 the same in LUMEX_PROTOCOL_CHECK_IDENTIFIERS (ERROR)
#   6 a layer row named after a platform macro (min)
#   7 two reply types
#   8 an option given twice
#   9 a value that is no option
#  10 a type that is no field in a field group
#  11 get_option of an option the row does not have
#  12 accepted_in with enumerators of two enumerations
#  13 a field with two scales
# The compiler's messages are matched in English. Everything runs at
# configure time (try_compile), at C++11 (MSVC: C++14, its lowest) and C++20.

cmake_minimum_required(VERSION 3.16)
project(LumexProtocolCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
# The DSL is header-only; the error category is not used here, so the
# headers compile without the library.
set(_includes "${LUMEX_SOURCE_DIR}")
if(MSVC)
    set(_werror /WX /Zc:__cplusplus /permissive- /EHsc)
    set(_all_standards 14 20)
else()
    set(_werror -Werror)
    set(_all_standards 11 20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # A --gcc-toolchain flag of the calling tree is unused when compiling.
        list(APPEND _werror -Wno-unused-command-line-argument)
    endif()
endif()

# _lumex_case(<n> "<regex of the message>")
set(_cases "")
function(_lumex_case number regex)
    set(_cases ${_cases} ${number} PARENT_SCOPE)
    set(_case_regex_${number} "${regex}" PARENT_SCOPE)
endfunction()

set(_names_regex "has a different number of field names than fields")
set(_macro_regex "is the name of a platform macro")
_lumex_case(1 "row SET_LEVEL ${_names_regex}")
_lumex_case(2 "row SET_LEVEL ${_names_regex}")
_lumex_case(3 "layout bad_layout ${_names_regex}")
_lumex_case(4 "row identifier DELETE ${_macro_regex}")
_lumex_case(5 "row identifier ERROR ${_macro_regex}")
_lumex_case(6 "row identifier min ${_macro_regex}")
_lumex_case(7 "a reply group holds at most one type")
_lumex_case(8 "a row option is given twice")
_lumex_case(9 "a row option must be timeout")
_lumex_case(10 "every element of a field group must be a field type")
_lumex_case(11 "get_option of an option the row does not have")
_lumex_case(12 "takes enumerators of one enumeration")
_lumex_case(13 "give scale<> at most once")

function(_lumex_protocol_try result_var tag standard)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${_source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${_includes}"
        COMPILE_DEFINITIONS ${_werror} ${ARGN}
        CXX_STANDARD ${standard}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_VARIABLE _output)
    set(${result_var} ${_ok} PARENT_SCOPE)
    set(${result_var}_OUTPUT "${_output}" PARENT_SCOPE)
endfunction()

set(_checked 0)
foreach(_std IN LISTS _all_standards)
    set(_tag "cxx${_std}")
    _lumex_protocol_try(_good good_${_tag} ${_std} -DLUMEX_PROTOCOL_GOOD_CASE)
    if(NOT _good)
        message(FATAL_ERROR
            "LUMEX_PROTOCOL_GOOD_CASE (${_tag}) must compile\n${_good_OUTPUT}")
    endif()

    foreach(_case IN LISTS _cases)
        _lumex_protocol_try(_bad bad_${_case}_${_tag} ${_std}
                            -DLUMEX_PROTOCOL_BAD_CASE=${_case})
        if(_bad)
            message(FATAL_ERROR
                "LUMEX_PROTOCOL_BAD_CASE=${_case} (${_tag}) compiled; the "
                "check that should reject it is missing")
        endif()
        # The case must fail with the message of its own check, not because
        # check.cpp has a typo.
        if(NOT _bad_OUTPUT MATCHES "${_case_regex_${_case}}")
            message(FATAL_ERROR
                "LUMEX_PROTOCOL_BAD_CASE=${_case} (${_tag}) failed, but "
                "not with /${_case_regex_${_case}}/\n${_bad_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "protocol compile checks: ${_checked} rejected cases as expected")
