# Compile-check fixture for cmake.hygiene_compile_checks.
#
# Header hygiene that holds at every standard: what a public header does must
# not depend on which other Lumex header came before it, and it must compile
# without a warning under the strict flags of a consumer. Each check is one
# translation unit built at configure time (try_compile, compile only), at
# C++11, C++14, C++17 and C++20 (MSVC: from C++14):
#
#   header_order.cpp            LumexTime before LumexGenerators and the other
#                               way round (lumex::core::time must not capture
#                               the C function time () that the number
#                               generator calls).
#   unused_result.cpp           LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR on a result of
#                               a LUMEX_ATTRIBUTE_NODISCARD function, with
#                               -Wall -Wextra -Werror (GCC reports a result
#                               that is only cast to void below C++17); the
#                               same result discarded without the macro must
#                               be rejected, so that the first check proves
#                               something.
#   variadic_macros_pedantic.cpp LUMEX_DEFINE_REFLECTED_ENUM, LUMEX_MEASURE_TIME
#                               with -Wall -Wextra -Wpedantic -Werror (no empty
#                               `...` argument).
#   field_reflection_warnings.cpp the field reflection header with -Wall -Wextra
#                               -Wpedantic -Werror (GCC reports the friend
#                               declaration of the C++14 loophole path with
#                               -Wnon-template-friend).
#   json_umbrella.cpp           the JSON umbrella with -Wall -Wextra -Wpedantic
#                               -Werror (LumexJsonSchemaValidator and
#                               LumexJsonSchemaNormalizer discard a nodiscard
#                               result on purpose; the reflected enums of the
#                               JSON headers pass no empty `...` argument).
#
# A check that fails reports the compiler output.

cmake_minimum_required(VERSION 3.16)
project(LumexHygieneCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

# The compiler's messages are matched in English below.
set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

# Compile only: the checks name no symbol of a compiled library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(MSVC)
    set(_flags /WX /Zc:__cplusplus /permissive- /EHsc)
    set(_standards 14 17 20)
    # The strict warning sets are GCC and Clang options.
    set(_strict "")
    set(_pedantic "")
else()
    set(_flags -Werror)
    set(_standards 11 14 17 20)
    set(_strict -Wall -Wextra)
    set(_pedantic -Wall -Wextra -Wpedantic)
endif()

# One translation unit of this directory at one standard; the result and the
# compiler output come back in <result_var> and <result_var>_OUTPUT.
function(_lumex_hygiene_try result_var tag source standard)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        CMAKE_FLAGS
            "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR};${LUMEX_SOURCE_DIR}/3rdparty"
        COMPILE_DEFINITIONS ${_flags} ${ARGN}
        CXX_STANDARD ${standard}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_VARIABLE _output)
    set(${result_var} ${_ok} PARENT_SCOPE)
    set(${result_var}_OUTPUT "${_output}" PARENT_SCOPE)
endfunction()

# A check that must compile; the message names the check and the standard.
function(_lumex_hygiene_expect_ok description source standard)
    string(MAKE_C_IDENTIFIER "${description}_${standard}" _tag)
    _lumex_hygiene_try(_result ${_tag} ${source} ${standard} ${ARGN})
    if(NOT _result)
        message(FATAL_ERROR
            "${description} (C++${standard}) must compile\n${_result_OUTPUT}")
    endif()
endfunction()

# A check that must not compile, and must fail for the reason named: the
# compiler output has to contain <pattern>, so that a typo in the source does
# not pass for the diagnostic.
function(_lumex_hygiene_expect_rejected description pattern source standard)
    string(MAKE_C_IDENTIFIER "${description}_${standard}" _tag)
    _lumex_hygiene_try(_result ${_tag} ${source} ${standard} ${ARGN})
    if(_result)
        message(FATAL_ERROR
            "${description} (C++${standard}) compiled; it must be rejected")
    endif()
    if(NOT _result_OUTPUT MATCHES "${pattern}")
        message(FATAL_ERROR
            "${description} (C++${standard}) failed, but not with '${pattern}'"
            "\n${_result_OUTPUT}")
    endif()
endfunction()

set(_compiled 0)
set(_rejected 0)
foreach(_std IN LISTS _standards)
    _lumex_hygiene_expect_ok("LumexTime before LumexGenerators"
        header_order.cpp ${_std} -DLUMEX_HYGIENE_TIME_FIRST)
    _lumex_hygiene_expect_ok("LumexGenerators before LumexTime"
        header_order.cpp ${_std} -DLUMEX_HYGIENE_GENERATORS_FIRST)
    math(EXPR _compiled "${_compiled} + 2")

    _lumex_hygiene_expect_ok("a discarded result through the macro"
        unused_result.cpp ${_std} ${_strict} -DLUMEX_HYGIENE_GOOD_CASE)
    math(EXPR _compiled "${_compiled} + 1")
    if(NOT MSVC)
        # MSVC reports a discarded result of the C++14 attribute only under
        # /analyze, and of [[nodiscard]] with a warning level it does not have
        # here.
        _lumex_hygiene_expect_rejected("a discarded result without the macro"
            "unused-result" unused_result.cpp ${_std} ${_strict}
            -DLUMEX_HYGIENE_BAD_DISCARD)
        math(EXPR _rejected "${_rejected} + 1")
    endif()

    _lumex_hygiene_expect_ok("macros that count their arguments"
        variadic_macros_pedantic.cpp ${_std} ${_pedantic})
    _lumex_hygiene_expect_ok("the field reflection header"
        field_reflection_warnings.cpp ${_std} ${_pedantic})
    _lumex_hygiene_expect_ok("the JSON umbrella"
        json_umbrella.cpp ${_std} ${_pedantic})
    math(EXPR _compiled "${_compiled} + 3")
endforeach()

message(STATUS "hygiene compile checks: ${_compiled} translation units "
               "compile, ${_rejected} are rejected as expected")
