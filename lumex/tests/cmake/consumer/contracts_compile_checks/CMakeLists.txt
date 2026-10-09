# Compile-check fixture for cmake.contracts_compile_checks.
#
# Every LUMEX_CONTRACTS_BAD_CASE in check.cpp must fail to compile with warnings
# as errors, and LUMEX_CONTRACTS_GOOD_CASE must compile with every semantic and
# with exceptions caught or not, under strict warnings. The cases are the uses
# the module rejects: a predicate that does not convert to bool (also with the
# semantic ignore, which does not evaluate it), no predicate, an assertion
# outside a function, a wrong or capturing handler, scoped enumerations
# converted from or to int, a copied scoped handler, a failing assertion in
# constant evaluation. A second group checks the macro LUMEX_CONTRACTS_SEMANTIC:
# a word that is not a semantic stops the compilation. The shared preamble of
# check.cpp compiles in the good case, so a bad case fails because of its own
# line. Checked at C++11, C++17 and C++20; everything runs at configure time
# (try_compile, objects only: the library is not linked).

cmake_minimum_required(VERSION 3.16)
project(LumexContractsCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

# The compiler's messages are matched in English below.
set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
if(MSVC)
    set(_werror /WX /W4 /Zc:__cplusplus /permissive- /EHsc)
    set(_standards 14 17 20)
else()
    set(_werror -Werror -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
                -Wshadow -Wold-style-cast)
    set(_standards 11 17 20)
endif()
set(_last_case 18)
# The cases that evaluate a constexpr function: they hold no lambda, so they
# are not compiled with exceptions caught.
set(_semantics ignore observe enforce quick_enforce p2900)

function(_lumex_contracts_try result_var tag standard)
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

    # Valid code compiles with every semantic, exceptions caught or not.
    foreach(_semantic IN LISTS _semantics)
        foreach(_catch 0 1)
            _lumex_contracts_try(_ok good_${_semantic}_${_catch}_${_tag} ${_std}
                -DLUMEX_CONTRACTS_GOOD_CASE
                -DLUMEX_CONTRACTS_SEMANTIC=${_semantic}
                -DLUMEX_CONTRACTS_CATCH_EXCEPTIONS=${_catch})
            if(NOT _ok)
                message(FATAL_ERROR
                    "LUMEX_CONTRACTS_GOOD_CASE (${_tag}, ${_semantic}, "
                    "catch ${_catch}) must compile\n${_ok_OUTPUT}")
            endif()
            math(EXPR _checked "${_checked} + 1")
        endforeach()
    endforeach()
    # And without any macro: the default semantic.
    _lumex_contracts_try(_ok good_default_${_tag} ${_std}
        -DLUMEX_CONTRACTS_GOOD_CASE)
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_CONTRACTS_GOOD_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
    endif()

    foreach(_case RANGE 1 ${_last_case})
        # Which semantic the bad case runs with: enforce, except that the
        # cases about ignore need the default.
        _lumex_contracts_try(_ok bad_${_case}_${_tag} ${_std}
                             -DLUMEX_CONTRACTS_BAD_CASE=${_case})
        if(_ok)
            message(FATAL_ERROR
                "LUMEX_CONTRACTS_BAD_CASE=${_case} (${_tag}) compiled; the "
                "check that should reject it is missing")
        endif()
        # A case must fail for its own reason, not because check.cpp has a
        # typo.
        if(_ok_OUTPUT MATCHES "is not one of the cases")
            message(FATAL_ERROR
                "LUMEX_CONTRACTS_BAD_CASE=${_case} is not defined in check.cpp")
        endif()
        if(_ok_OUTPUT MATCHES "not declared in this scope|undeclared identifier|use of undeclared|C2065")
            message(FATAL_ERROR
                "LUMEX_CONTRACTS_BAD_CASE=${_case} (${_tag}) fails on a missing "
                "name, not on the check\n${_ok_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()

    # A word that is not a semantic stops the compilation with the message of
    # the header (the empty word, upper case and a number included).
    foreach(_word "fast" "ENFORCE" "2" "")
        _lumex_contracts_try(_ok word_${_tag} ${_std}
            -DLUMEX_CONTRACTS_GOOD_CASE "-DLUMEX_CONTRACTS_SEMANTIC=${_word}")
        if(_ok)
            message(FATAL_ERROR
                "LUMEX_CONTRACTS_SEMANTIC='${_word}' (${_tag}) compiled")
        endif()
        if(NOT _ok_OUTPUT MATCHES "LUMEX_CONTRACTS_SEMANTIC must be one of")
            message(FATAL_ERROR
                "LUMEX_CONTRACTS_SEMANTIC='${_word}' (${_tag}) failed for "
                "another reason\n${_ok_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "contracts compile checks: ${_checked} cases as expected")
