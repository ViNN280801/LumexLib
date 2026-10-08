# Compile-check fixture for cmake.hygiene_compile_checks.
#
# Header hygiene that holds at every standard: what a public header does must
# not depend on which other Lumex header came before it, and it must compile
# without a warning under the strict flags of a consumer. Each check is one
# translation unit built at configure time (try_compile, compile only), at
# C++11, C++14, C++17 and C++20 (MSVC: from C++14):
#
#   header_order.cpp  LumexTime before LumexGenerators and the other way round
#                     (lumex::core::time must not capture the C function
#                     time () that the number generator calls).
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
else()
    set(_flags -Werror)
    set(_standards 11 14 17 20)
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

set(_checked 0)
foreach(_std IN LISTS _standards)
    _lumex_hygiene_expect_ok("LumexTime before LumexGenerators"
        header_order.cpp ${_std} -DLUMEX_HYGIENE_TIME_FIRST)
    _lumex_hygiene_expect_ok("LumexGenerators before LumexTime"
        header_order.cpp ${_std} -DLUMEX_HYGIENE_GENERATORS_FIRST)
    math(EXPR _checked "${_checked} + 2")
endforeach()

message(STATUS "hygiene compile checks: ${_checked} translation units compile")
