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
#  14 a row identifier that is a C++ keyword (delete)
#  15 a row identifier that is a macro of the translation unit
#  16 a field group naming a type hidden by a vocabulary word (stream)
#  17 a reply naming a vocabulary word (level)
#  18 a reply naming the row's own type (row status, layout status_t)
#  19 a reply that is no field, layout or reply kind
#  20 a misaligned row of binary_layout<natural_alignment<0> >
#  21 a floating field in a decimal fixed-width layout
#  22 an optional tail that is not the last field
#  23 the reply of a row without reply
#  24 a reply kind of the dialogue decoded by the core
#  25 a request with the wrong number of arguments
#  26 two parts of a split sharing a bit
#  27 a fractional argument for an integer field without scale
#  28 a header vocabulary that does not derive from dsl::vocabulary
#  29 a layout that names itself among its fields
#  30 a registry key given an enumerator of another registry
#  31 two rows with one integer key on the wire, asked for the index
#  32 a registry row with two types
#  33 a registry row named after a platform macro without P_AS
#  34 a registry value given a string
#  35 the index of a binary layer without a key width or a key reader
#  36 a registry with a floating entry used by registry_value_
#  37 a header whose write_header is private
#  38 a header whose write_header has the wrong parameters
#  39 a header whose write_header is no static member
#  40 a tokenized layer whose header names write_header
#  41 dispatch_message on a layer without a key reader
#  42 encode_message on a layer whose key reader has no writer twin
#  43 a reply kind whose describe hook has the wrong parameters
#  44 a reply kind whose text is no static function
#  45 an option of another layer whose text is no member function
#  46 a vocabulary whose protocol check is no member template
#  47 a vocabulary whose protocol check refuses the table
#  48 an option of another layer named like a key of the core options
#  49 an option of another layer whose option_repeats is no type
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
_lumex_case(14 "row identifier delete is a C\\+\\+ keyword")
_lumex_case(15 "row identifier WIDGET_CODE is a macro in this translation unit")
_lumex_case(16 "field group of the row PING names a word of the table vocabulary")
_lumex_case(17 "reply of the row PING names a word of the table vocabulary")
_lumex_case(18 "reply of the row status names the row's own type status_t")
_lumex_case(19 "reply of the row PING is not a field, a layout or a reply kind")
_lumex_case(20 "a 2-byte field is not at an even offset")
_lumex_case(21 "a floating field in a decimal fixed-width layout")
_lumex_case(22 "optional_tail_ must be the last field of its group")
_lumex_case(23 "the row has no reply")
_lumex_case(24 "is a reply kind of the dialogue")
_lumex_case(25 "give one argument per field of the row")
_lumex_case(26 "two parts of split_<Word, ...> share a bit")
_lumex_case(27 "an integer field without scale<> takes an integer")
_lumex_case(28 "the lumex_vocabulary of a header derives from dsl::vocabulary")
_lumex_case(29 "a field of the layout node_t names its own type")
_lumex_case(30 "a registry_key_ takes an enumerator of its own registry")
_lumex_case(31 "two rows of the protocol have the same integer key on the wire")
_lumex_case(32 "the type group of a row holds exactly one field")
_lumex_case(33 "row identifier DELETE ${_macro_regex}")
_lumex_case(34 "a registry_value_ takes an engineering value")
_lumex_case(35 "names the width of the key on the wire")
_lumex_case(36 "every entry of the registry has an integer")
set(_header_regex "names write_header, but not as an accessible static")
_lumex_case(37 "${_header_regex}")
_lumex_case(38 "${_header_regex}")
_lumex_case(39 "${_header_regex}")
_lumex_case(40 "a tokenized layer writes its key itself")
_lumex_case(41 "the layer has no key reader")
_lumex_case(42 "the key reader of the layer has no writer twin")
_lumex_case(43 "the reply kind names lumex_describe_reply, but not as")
_lumex_case(44 "the reply kind names lumex_reply_text, but not as")
_lumex_case(45 "the option names view_text, but not as")
_lumex_case(46 "the vocabulary names lumex_check_protocol, but not as")
_lumex_case(47 "the vocabulary of the protocol refuses its table")
_lumex_case(48 "is a constexpr name other than the keys of the core")
_lumex_case(49 "the option names option_repeats, but not")

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
