# Compile-check fixture for cmake.expected_compile_checks.
#
# The expected umbrella has no global `unexpected` and no global `in_place`
# (MAJOR 2.0.0.0; the MinGW runtime declares a global function `unexpected` in
# <eh.h>, and optional has its own `in_place`). The fixture checks it three
# ways in check.cpp:
#   LUMEX_EXPECTED_GOOD_CASE        compiles: the names that stay global
#                                   (expected, bad_expected_access,
#                                   make_unexpected, unexpect, unexpect_t,
#                                   in_place_tag) are the names of the module,
#                                   and unexpected is reached by its full name
#   LUMEX_EXPECTED_OWN_NAME_CASE    1-4 compile: the program's own global
#                                   `unexpected` (a function as in <eh.h>, a
#                                   class template as in other libraries)
#                                   declared before or after the umbrella
#   LUMEX_EXPECTED_BAD_CASE         1-4 fail to compile with the name missing:
#                                   ::unexpected<int>, unexpected<int> with no
#                                   using, a using-declaration of
#                                   ::unexpected, ::in_place
# The checks run at C++11, C++17 and C++20. Everything runs at configure time
# (try_compile).

cmake_minimum_required(VERSION 3.16)
project(LumexExpectedCompileChecks LANGUAGES CXX)

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
# The name each bad case must report as missing.
set(_bad_names unexpected unexpected unexpected in_place)
set(_last_own_case 4)

function(_lumex_expected_try result_var tag standard)
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
    _lumex_expected_try(_ok good_${_tag} ${_std} -DLUMEX_EXPECTED_GOOD_CASE)
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_EXPECTED_GOOD_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
    endif()

    foreach(_case RANGE 1 ${_last_own_case})
        _lumex_expected_try(_ok own_${_case}_${_tag} ${_std}
                            -DLUMEX_EXPECTED_OWN_NAME_CASE=${_case})
        if(NOT _ok)
            message(FATAL_ERROR
                "LUMEX_EXPECTED_OWN_NAME_CASE=${_case} (${_tag}) must "
                "compile: the umbrella clashes with the program's own "
                "unexpected\n${_ok_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()

    set(_case 0)
    foreach(_name IN LISTS _bad_names)
        math(EXPR _case "${_case} + 1")
        _lumex_expected_try(_ok bad_${_case}_${_tag} ${_std}
                            -DLUMEX_EXPECTED_BAD_CASE=${_case})
        if(_ok)
            message(FATAL_ERROR
                "LUMEX_EXPECTED_BAD_CASE=${_case} (${_tag}) compiled; the "
                "umbrella still has a global ${_name}")
        endif()
        # A case must fail for its own reason, not because check.cpp has a
        # typo: the error must be the one for the missing name (GCC, Clang
        # and MSVC spell it differently), not the #error of an unknown case.
        if(_ok_OUTPUT MATCHES "is not one of the cases")
            message(FATAL_ERROR
                "LUMEX_EXPECTED_BAD_CASE=${_case} is not defined in check.cpp")
        endif()
        if(NOT _ok_OUTPUT MATCHES
           "'(::)?${_name}' (has not been declared|was not declared)|(no template named|no member named|undeclared identifier) '${_name}'|C2065|C2039|C2653")
            message(FATAL_ERROR
                "LUMEX_EXPECTED_BAD_CASE=${_case} (${_tag}) fails, but not "
                "on the missing name ${_name}\n${_ok_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "expected compile checks: ${_checked} cases as expected")
