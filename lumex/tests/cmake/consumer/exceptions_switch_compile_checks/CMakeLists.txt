# Compile-check fixture of cmake.consumer_exceptions_switch_compile_checks.
#
# The switch of the library, LUMEX_HAS_EXCEPTIONS, and the macro of each module
# that defaults to it (expected, optional, span, string_view) refuse the value 1
# when the compiler has exceptions off (-fno-exceptions; MSVC /EHs-c- with
# _HAS_EXCEPTIONS=0): a program built without exceptions cannot throw. For every
# macro of the list below, at each standard:
#   - the macro defined as 1 without exceptions must NOT compile, and the error
#     must be the #error of that very macro (not some other failure);
#   - the macro defined as 0 without exceptions must compile;
#   - the macro defined as 1 with exceptions on must compile (so the refusal is
#     the compiler mode, not the definition);
#   - the header set without any definition compiles without exceptions.
# Everything runs at configure time (try_compile), the files are compiled and
# not run. The runtime side (the values of the macros and what each module does
# in each mode) is the exceptions_switch fixture.

cmake_minimum_required(VERSION 3.16)
project(LumexExceptionsSwitchCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

# The compiler's messages are matched in English below.
set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
if(MSVC)
    set(_with_exceptions /EHsc /Zc:__cplusplus /permissive-)
    # The later /EHs-c- wins over the /EHsc of CMAKE_CXX_FLAGS (warning D9025).
    set(_without_exceptions /EHs-c- /D_HAS_EXCEPTIONS=0 /Zc:__cplusplus /permissive-)
    set(_standards 14 17 20)
else()
    set(_with_exceptions "")
    set(_without_exceptions -fno-exceptions)
    set(_standards 11 17 20)
endif()
set(_macros
    LUMEX_HAS_EXCEPTIONS
    LUMEX_EXPECTED_HAS_EXCEPTIONS
    LUMEX_OPTIONAL_HAS_EXCEPTIONS
    LUMEX_SPAN_HAS_EXCEPTIONS
    LUMEX_STRING_VIEW_HAS_EXCEPTIONS)

# The files are compiled, not run: a static library needs no handler.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

function(_lumex_try result_var tag standard)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${_source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
        COMPILE_DEFINITIONS ${ARGN}
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

    # The header set compiles without exceptions and without a definition.
    _lumex_try(_ok base_${_tag} ${_std} ${_without_exceptions})
    if(NOT _ok)
        message(FATAL_ERROR
            "the headers must compile without exceptions (${_tag})\n${_ok_OUTPUT}")
    endif()

    foreach(_macro IN LISTS _macros)
        # 1 without exceptions: refused by the #error of that macro.
        _lumex_try(_ok one_${_macro}_${_tag} ${_std}
                   ${_without_exceptions} -D${_macro}=1)
        if(_ok)
            message(FATAL_ERROR
                "${_macro}=1 without exceptions compiled (${_tag}); the #error "
                "that should refuse it is missing")
        endif()
        if(NOT _ok_OUTPUT MATCHES "[\" ]${_macro} is 1 but the compiler has exceptions off")
            message(FATAL_ERROR
                "${_macro}=1 without exceptions (${_tag}) failed, but not on the "
                "#error of ${_macro}\n${_ok_OUTPUT}")
        endif()

        # 0 without exceptions: the same as the default.
        _lumex_try(_ok zero_${_macro}_${_tag} ${_std}
                   ${_without_exceptions} -D${_macro}=0)
        if(NOT _ok)
            message(FATAL_ERROR
                "${_macro}=0 without exceptions must compile (${_tag})\n${_ok_OUTPUT}")
        endif()

        # 1 with exceptions: allowed, so the refusal above is the compiler mode.
        _lumex_try(_ok on_${_macro}_${_tag} ${_std}
                   ${_with_exceptions} -D${_macro}=1)
        if(NOT _ok)
            message(FATAL_ERROR
                "${_macro}=1 with exceptions must compile (${_tag})\n${_ok_OUTPUT}")
        endif()

        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "exceptions switch compile checks: ${_checked} macro cases as expected")
