# Compile-check fixture for cmake.smart_ptr_compile_checks.
#
# Every LUMEX_SP_BAD_CASE in check.cpp must fail to compile with warnings as
# errors, LUMEX_SP_GOOD_CASE must compile, and so must LUMEX_SP_ALLOW_CASE
# with LUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS defined. The cases
# are the uses the interface rejects: a pointer of the wrong type or a
# conversion the constraints remove (1-3, 10-12, 17-19, 28, 29), implicit
# conversions to or from std::shared_ptr (4, 5, 27), a class with only the
# standard enable_shared_from_this base (6, 7), the array forms of
# make_shared (8, 9), operators that an element type does not have (13-16),
# casts the language forbids (20-23), missing functions (24-26, 30, 31). The
# checks run at C++11, C++17 and C++20. Everything runs at configure time
# (try_compile).

cmake_minimum_required(VERSION 3.16)
project(LumexSmartPtrCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

# The compiler's messages are matched in English below.
set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

# Objects only: a debug build of the headers calls the assert handler of the
# compiled utility library, which this fixture does not link.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
if(MSVC)
    set(_werror /WX /Zc:__cplusplus /permissive- /EHsc)
    set(_standards 14 17 20)
else()
    set(_werror -Werror)
    set(_standards 11 17 20)
endif()
set(_last_case 31)

function(_lumex_smart_ptr_try result_var tag standard)
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
    _lumex_smart_ptr_try(_ok good_${_tag} ${_std} -DLUMEX_SP_GOOD_CASE)
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_SP_GOOD_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
    endif()

    _lumex_smart_ptr_try(_ok allow_${_tag} ${_std} -DLUMEX_SP_ALLOW_CASE
        -DLUMEX_SMART_PTR_ALLOW_STD_ENABLE_SHARED_FROM_THIS)
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_SP_ALLOW_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
    endif()

    foreach(_case RANGE 1 ${_last_case})
        _lumex_smart_ptr_try(_ok bad_${_case}_${_tag} ${_std}
                        -DLUMEX_SP_BAD_CASE=${_case})
        if(_ok)
            message(FATAL_ERROR
                "LUMEX_SP_BAD_CASE=${_case} (${_tag}) compiled; the "
                "check that should reject it is missing")
        endif()
        # A case must fail for its own reason, not because check.cpp has a
        # typo: the error text must not be the #error of an unknown case or a
        # missing name (GCC, Clang and MSVC spell the second one differently).
        if(_ok_OUTPUT MATCHES "is not one of the cases")
            message(FATAL_ERROR
                "LUMEX_SP_BAD_CASE=${_case} is not defined in check.cpp")
        endif()
        if(_ok_OUTPUT MATCHES "not declared in this scope|undeclared identifier|use of undeclared|C2065")
            message(FATAL_ERROR
                "LUMEX_SP_BAD_CASE=${_case} (${_tag}) fails on a missing "
                "name, not on the check\n${_ok_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "smart_ptr compile checks: ${_checked} rejected cases as expected")
