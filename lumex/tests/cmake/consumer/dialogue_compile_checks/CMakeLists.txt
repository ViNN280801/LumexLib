# Compile-check fixture for cmake.dialogue_compile_checks.
#
# The dialogue vocabulary (lumex/applied/transport/protocol/dsl) must refuse
# a table the dialogue cannot use with the message of its own check, most of
# them when the index of the table is built (the index asks the vocabulary
# for its checks). Every LUMEX_DIALOGUE_BAD_CASE in check.cpp must fail to
# compile with that message, and LUMEX_DIALOGUE_GOOD_CASE (the same rows,
# valid) must compile:
#   1 an ack_nak whose Ack is no field, layout or row
#   2 an ack_nak whose Ack and Nak are the same
#   3 reply_row naming a row of another protocol
#   4 reply_row of a type that is no row
#   5 edge_event on a row without stream
#   6 edge_event with a field index out of range
#   7 state_from with a field index out of range
#   8 state_from naming a row of another protocol
#   9 correlation::by_reply_key without reply_row
#  10 reply_row with another correlation
#  11 correlation::custom without match_with
#  12 a type in dialogue::options<...> that is no protocol option
#  13 a protocol option given twice
#  14 max_in_flight<0>
#  15 watchdog_ms<0>
#  16 a keepalive with a period of 0
#  17 a keepalive naming a row of another protocol
#  18 identify_with a row without a reply
#  19 dialogue_options that is no dialogue::options<...>
#  20 dialogue_options the dialogue cannot read (private)
#  21 echo_zero_on_error of a layout
#  22 an edge from a value to the same value
#  23 an edge of a text field
#  24 reply_row on a row with a reply group of its own
#  25 an acknowledgement row whose reply is itself a reply kind
#  26 match_with with another correlation
#  27 an acknowledgement row of another protocol
#  28 identify_with a row of another protocol
#  29 a two_phase whose Ack is no field, layout or row
#  30 an echo_set of a type that is no field
#  31 a layer that replaces the row of reply_row without listing its row
#  32 a layer that replaces an acknowledgement row without listing its row
#  33 a protocol option naming a row the layer replaces
#  34 correlation::by_key on a row whose reply kind names a row
#  35 a by_order row in a protocol with max_in_flight<2>
#  36 a correlation outside the enumeration
#  37 a keepalive period not shorter than the watchdog
# The compiler's messages are matched in English. The headers are compiled
# without the library. Everything runs at configure time (try_compile), at
# C++11 (MSVC: C++14, its lowest) and C++20.

cmake_minimum_required(VERSION 3.16)
project(LumexDialogueCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
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

# The file is compiled, not run: a static library needs no main and no
# error category of the library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# _lumex_case(<n> "<regex of the message>")
set(_cases "")
function(_lumex_case number regex)
    set(_cases ${_cases} ${number} PARENT_SCOPE)
    set(_case_regex_${number} "${regex}" PARENT_SCOPE)
endfunction()

set(_another_regex "names a row of another protocol")
_lumex_case(1 "ack_nak<Ack, Nak>: Ack is a field, a layout or a row of the protocol")
_lumex_case(2 "ack_nak<Ack, Nak>: Ack and Nak are different")
_lumex_case(3 "reply_row<R> ${_another_regex}")
_lumex_case(4 "reply_row<R>: R is a row of the protocol")
_lumex_case(5 "edge_event on a row without stream")
_lumex_case(6 "Field is not a field of the reply of the row")
_lumex_case(7 "state_from<Row, Field>: Field is not a field of the reply of Row")
_lumex_case(8 "state_from<Row, Field> ${_another_regex}")
_lumex_case(9 "correlation::by_reply_key needs reply_row<R>")
_lumex_case(10 "reply_row<R> needs correlation::by_reply_key")
_lumex_case(11 "correlation::custom needs match_with")
_lumex_case(12 "every element of dialogue::options<...> is a protocol option")
_lumex_case(13 "a protocol option is given twice")
_lumex_case(14 "max_in_flight<N> needs N of at least 1")
_lumex_case(15 "watchdog_ms<Ms> needs Ms above 0")
_lumex_case(16 "keepalive<Row, PeriodMs> needs PeriodMs above 0")
_lumex_case(17 "keepalive<Row, PeriodMs> ${_another_regex}")
_lumex_case(18 "identify_with<Row>: the row has a reply of the core")
_lumex_case(19 "the dialogue_options of a header is a dialogue::options")
_lumex_case(20 "the header names dialogue_options, but not as")
_lumex_case(21 "echo_zero_on_error<T> takes one field whose value is a number")
_lumex_case(22 "needs From and To different")
_lumex_case(23 "the field is an integer, a flag or an enumeration")
_lumex_case(24 "a row with reply_row<R> leaves its reply group empty")
_lumex_case(25 "names a row whose reply is itself a reply kind")
_lumex_case(26 "match_with needs correlation::custom")
_lumex_case(27 "a reply kind of the row ${_another_regex}")
_lumex_case(28 "identify_with<Row> ${_another_regex}")
_lumex_case(29 "two_phase<Ack, Completion>: Ack is a field, a layout or a row")
_lumex_case(30 "echo_set<T>: T is a field or a layout")
_lumex_case(31 "reply_row<R> names a row the layer replaced: list the row")
_lumex_case(32 "a reply kind of the row names a row the layer replaced")
_lumex_case(33 "keepalive<Row, PeriodMs> names a row the layer replaced")
_lumex_case(34 "correlation::by_key on a row whose reply kind names a row")
_lumex_case(35 "a by_order row in a protocol with max_in_flight greater than 1")
_lumex_case(36 "correlate \\(how\\) takes a correlation")
_lumex_case(37 "with a period not shorter than watchdog_ms")

function(_lumex_dialogue_try result_var tag standard)
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
    _lumex_dialogue_try(_good good_${_tag} ${_std} -DLUMEX_DIALOGUE_GOOD_CASE)
    if(NOT _good)
        message(FATAL_ERROR
            "LUMEX_DIALOGUE_GOOD_CASE (${_tag}) must compile\n${_good_OUTPUT}")
    endif()

    foreach(_case IN LISTS _cases)
        _lumex_dialogue_try(_bad bad_${_case}_${_tag} ${_std}
                            -DLUMEX_DIALOGUE_BAD_CASE=${_case})
        if(_bad)
            message(FATAL_ERROR
                "LUMEX_DIALOGUE_BAD_CASE=${_case} (${_tag}) compiled; the "
                "check that should reject it is missing")
        endif()
        # The case must fail with the message of its own check, not because
        # check.cpp has a typo.
        if(NOT _bad_OUTPUT MATCHES "${_case_regex_${_case}}")
            message(FATAL_ERROR
                "LUMEX_DIALOGUE_BAD_CASE=${_case} (${_tag}) failed, but "
                "not with /${_case_regex_${_case}}/\n${_bad_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "dialogue compile checks: ${_checked} rejected cases as expected")
