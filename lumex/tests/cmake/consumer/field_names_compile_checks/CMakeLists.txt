# Compile-check fixture for cmake.field_names_compile_checks.
#
# LUMEX_DEFINE_FIELD_NAMES registers the fields of an aggregate, and the
# registered field reflection (names_as_array, get, to_json) must refuse what
# it cannot serve. Every LUMEX_FIELD_NAMES_BAD_CASE in check.cpp must fail to
# compile with the message of its own check, and LUMEX_FIELD_NAMES_GOOD_CASE
# must compile, at C++11, C++14, C++17 and C++20:
#   1 names_as_array of an unregistered aggregate (C++11, 14, 17)
#   2 get of an unregistered aggregate (C++11)
#   3 to_json of an unregistered aggregate (C++11, 14, 17)
#   4 fewer names than fields
#   5 more names than fields
#   6 a name that is not a member
#   7 an array member (counted as several fields)
#   8 an index past the last field (not at C++14: the return type of the
#     C++14 get of the loophole is a hard error before the static_assert)
#   9 more than 32 names
#  10 the same aggregate registered twice
#  11 a private member
#  12 a reference member
#  13 to_json of an aggregate with a nested aggregate that has no names
#     (C++11, 14, 17; from C++20 the compiler supplies them)
#  14 to_json of a map of aggregates whose keys are not strings
# The compiler's messages are matched in English. At C++20 an unregistered
# aggregate has names (GCC 8 reports 201709L at -std=c++2a and is the
# exception, hence the cases 1 and 3 are not run at C++20). Everything runs at
# configure time (try_compile).

cmake_minimum_required(VERSION 3.16)
project(LumexFieldNamesCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
# LumexFieldReflection.hpp includes <nlohmann/json.hpp> from 3rdparty.
set(_includes "${LUMEX_SOURCE_DIR};${LUMEX_SOURCE_DIR}/3rdparty")
if(MSVC)
    set(_werror /WX /Zc:__cplusplus /permissive- /EHsc)
    set(_all_standards 14 17 20)
else()
    set(_werror -Werror)
    set(_all_standards 11 14 17 20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # A --gcc-toolchain flag of the calling tree is unused when compiling.
        list(APPEND _werror -Wno-unused-command-line-argument)
    endif()
endif()

# _lumex_case(<n> "<standards>" "<regex of the message>")
set(_cases "")
function(_lumex_case number standards regex)
    set(_cases ${_cases} ${number} PARENT_SCOPE)
    set(_case_standards_${number} "${standards}" PARENT_SCOPE)
    set(_case_regex_${number} "${regex}" PARENT_SCOPE)
endfunction()

_lumex_case(1 "11;14;17" "register them with LUMEX_DEFINE_FIELD_NAMES")
_lumex_case(2 "11" "get<I> below C\\+\\+14 needs the members of the aggregate")
_lumex_case(3 "11;14;17" "to_json needs the field names of the aggregate")
_lumex_case(4 "${_all_standards}" "the number of names differs from the number")
_lumex_case(5 "${_all_standards}" "the number of names differs from the number")
_lumex_case(6 "${_all_standards}" "is not a member of|no member named")
_lumex_case(7 "${_all_standards}" "the number of names differs from the number|layout mismatch")
_lumex_case(8 "11;17;20" "field index out of range")
_lumex_case(9 "${_all_standards}" "LUMEX_DEFINE_FIELD_NAMES_TAKES_AT_MOST_32_NAMES")
_lumex_case(10 "${_all_standards}" "redefinition|already has a body|C2084")
_lumex_case(11 "${_all_standards}" "private within this context|is a private member|C2248")
_lumex_case(12 "${_all_standards}" "reference member|reference type|pointer to reference")
_lumex_case(13 "11;14;17" "to_json needs the field names of the aggregate")
_lumex_case(14 "${_all_standards}" "only with keys that std::string can be made from")

function(_lumex_names_try result_var tag standard)
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
    _lumex_names_try(_good good_${_tag} ${_std} -DLUMEX_FIELD_NAMES_GOOD_CASE)
    if(NOT _good)
        message(FATAL_ERROR
            "LUMEX_FIELD_NAMES_GOOD_CASE (${_tag}) must compile\n${_good_OUTPUT}")
    endif()

    foreach(_case IN LISTS _cases)
        if(NOT _std IN_LIST _case_standards_${_case})
            continue()
        endif()
        _lumex_names_try(_bad bad_${_case}_${_tag} ${_std}
                         -DLUMEX_FIELD_NAMES_BAD_CASE=${_case})
        if(_bad)
            message(FATAL_ERROR
                "LUMEX_FIELD_NAMES_BAD_CASE=${_case} (${_tag}) compiled; the "
                "check that should reject it is missing")
        endif()
        # The case must fail with the message of its own check, not because
        # check.cpp has a typo.
        if(NOT _bad_OUTPUT MATCHES "${_case_regex_${_case}}")
            message(FATAL_ERROR
                "LUMEX_FIELD_NAMES_BAD_CASE=${_case} (${_tag}) failed, but "
                "not with /${_case_regex_${_case}}/\n${_bad_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "field names compile checks: ${_checked} rejected cases as expected")
