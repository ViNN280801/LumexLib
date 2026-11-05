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
#  38 an echo_set whose type cannot be compared with the last request field
#  39 an echo_zero_on_error whose type cannot be compared with it
#  40 enables_stream naming a row of another protocol
#  41 enables_stream of a type that is no row
#  42 enables_stream naming a row without stream
#  43 enables_stream on a row with request fields
#  44 enables_stream on a dangerous row
#  45 a keepalive naming a dangerous row
#  46 a layer that replaces the stream of enables_stream without listing it
#  47 keepalive_from on a protocol without a keepalive
#  48 a layer_set without layers
#  49 a layer_set of a type that is no layer
#  50 a layer_set naming a layer of another protocol
#  51 a layer_set naming a layer twice
# The generated client (LumexProtocolClient.hpp):
#  52 a dangerous row without confirm
#  53 confirm on a row that is not dangerous
#  54 too few arguments
#  55 too many arguments
#  56 one more argument, which is no deadline and no token
#  57 as<L> of a layer that is no layer of the client
#  58 identify without identify_with
#  59 identify without identity_of_t
#  60 identify by a row with request fields
#  61 on<Row> of a row whose reply is a reply kind
#  62 a row of another protocol
#  63 a dangerous row without confirm, in the async form
#  64 a row named like a member function of the client
#  65 a row named like an overloaded member
#  66 a row named like a member template
#  67 a row of the base layer named like a member
#  68 a row named like the class
#  69 a row named like a member type
#  70 a row of a layer named like a member of client_layer
#  71 a row of a layer named like the class client_layer
# A reply kind naming a row with stream:
#  72 a two_phase whose completion is a row with stream
#  73 a two_phase whose acknowledgement is a row with stream
#  74 an ack_nak whose positive part is a row with stream
#  75 an ack_nak whose negative part is a row with stream
#  76 a row of a layer naming a row with stream that the layer inherits
#  77 a layer that replaces an acknowledgement row with a row with stream
#  78 the same without listing the row that names it
#  79 async_discover over a table without identify_with
#  80 a row named like on_reconnect
#  81 a row named like identity
#  82 the view of a temporary stream_session
#  83 the view of a temporary message_session
# The good case also holds a row with reply_row naming a row with stream and a
# two_phase naming a row without it.
# The compiler's messages are matched in English. The headers are compiled
# without the library (the Asio headers only). Everything runs at configure
# time (try_compile), at C++11 (MSVC: C++14, its lowest) and C++20.

cmake_minimum_required(VERSION 3.16)
project(LumexDialogueCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
set(_includes "${LUMEX_SOURCE_DIR}")
# The reader of the reply kinds (case 38, 39) and the features of a table (case
# 47) include the dialogue types, which include Asio (headers only, no
# library).
set(_asio_include "${LUMEX_SOURCE_DIR}/3rdparty/asio/include")
if(MSVC)
    set(_werror /WX /Zc:__cplusplus /permissive- /EHsc /DASIO_STANDALONE
        /DASIO_NO_DEPRECATED "/I${_asio_include}")
    set(_all_standards 14 20)
else()
    set(_werror -Werror -DASIO_STANDALONE -DASIO_NO_DEPRECATED
        -isystem "${_asio_include}")
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
_lumex_case(38 "the echo of an echo kind can be compared with the last field")
_lumex_case(39 "the echo of an echo kind can be compared with the last field")
_lumex_case(40 "enables_stream<R> ${_another_regex}")
_lumex_case(41 "enables_stream<R>: R is a row of the protocol")
_lumex_case(42 "enables_stream<R>: R is a row with a stream option")
_lumex_case(43 "enables_stream<R> on a row with request fields")
_lumex_case(44 "enables_stream<R> on a dangerous row")
_lumex_case(45 "keepalive<Row, PeriodMs> on a dangerous row")
_lumex_case(46 "enables_stream<R> names a row the layer replaced")
_lumex_case(47 "keepalive_from<P>: the protocol names no")
_lumex_case(48 "layer_set<Layers...> names at least one")
_lumex_case(49 "layer_set<Layers...>: every element is a")
_lumex_case(50 "layer_set<Layers...> names a layer of")
_lumex_case(51 "layer_set<Layers...> names a layer twice")
_lumex_case(52 "a dangerous row needs confirm")
_lumex_case(53 "confirm on a row that is not")
_lumex_case(54 "give one argument per request")
_lumex_case(55 "give one argument per request")
_lumex_case(56 "give one argument per request")
_lumex_case(57 "as<L> \\(\\): L is no layer of this client")
_lumex_case(58 "the protocol names no")
_lumex_case(59 "specialize identity_of_t<P>")
_lumex_case(60 "has no request fields")
_lumex_case(61 "a row whose reply is a")
_lumex_case(62 "the row is no row of the layer")
_lumex_case(63 "a dangerous row needs confirm")
set(_hidden_regex "a row is named like a member of")
_lumex_case(64 "${_hidden_regex} client<P>")
_lumex_case(65 "${_hidden_regex} client<P>")
_lumex_case(66 "${_hidden_regex} client<P>")
_lumex_case(67 "${_hidden_regex} client<P>")
_lumex_case(68 "${_hidden_regex} client<P>")
_lumex_case(69 "${_hidden_regex} client<P>")
_lumex_case(70 "${_hidden_regex} client_layer<Owner, L>")
_lumex_case(71 "${_hidden_regex} client_layer<Owner, L>")
set(_stream_regex "names a row with stream: use reply_row<R>")
_lumex_case(72 "${_stream_regex}")
_lumex_case(73 "${_stream_regex}")
_lumex_case(74 "${_stream_regex}")
_lumex_case(75 "${_stream_regex}")
_lumex_case(76 "${_stream_regex}")
_lumex_case(77 "${_stream_regex}")
_lumex_case(78 "a reply kind of the row names a row the layer replaced")
_lumex_case(79 "async_discover: the protocol names no identify_with")
_lumex_case(80 "${_hidden_regex} client<P>")
_lumex_case(81 "${_hidden_regex} client<P>")
_lumex_case(82 "deleted (member )?function")
_lumex_case(83 "deleted (member )?function")

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
