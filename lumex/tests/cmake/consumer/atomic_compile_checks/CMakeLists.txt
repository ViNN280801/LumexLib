# Compile-check fixture for LumexCMake.atomic_compile_checks.
#
# Every LUMEX_ATOMIC_BAD_CASE in check.cpp must fail to compile with warnings
# as errors, and LUMEX_ATOMIC_GOOD_CASE must compile. Cases 1-4 discard the
# result of load () or is_lock_free () (nodiscard); cases 5-26 pass a constant
# memory order that the standard forbids, which only a compiler with the
# diagnose_if attribute (Clang) reports, so they are skipped elsewhere. The
# checks run at C++11 and C++20, and at C++20 with the forced lock-based
# implementation (MSVC: C++20 only; its C++14 nodiscard needs /analyze).
# Everything runs at configure time (try_compile).

cmake_minimum_required(VERSION 3.16)
project(LumexAtomicCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

find_package(Threads REQUIRED)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
if(MSVC)
    set(_werror /WX /Zc:__cplusplus /permissive- /EHsc)
    set(_standards 20)
else()
    set(_werror -Werror)
    set(_standards 11 20)
endif()

function(_lumex_atomic_try result_var tag standard)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${_source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
        COMPILE_DEFINITIONS ${_werror} ${ARGN}
        LINK_LIBRARIES Threads::Threads
        CXX_STANDARD ${standard}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_VARIABLE _output)
    set(${result_var} ${_ok} PARENT_SCOPE)
    set(${result_var}_OUTPUT "${_output}" PARENT_SCOPE)
endfunction()

set(_checked 0)
foreach(_variant IN ITEMS "plain" "forced")
    foreach(_std IN LISTS _standards)
        if(_variant STREQUAL "forced")
            if(NOT _std EQUAL 20)
                continue()
            endif()
            set(_extra -DLUMEX_FORCE_LOCK_BASED_ATOMIC_SHARED_PTR)
        else()
            set(_extra "")
        endif()
        set(_tag "${_variant}_cxx${_std}")

        _lumex_atomic_try(_ok good_${_tag} ${_std} -DLUMEX_ATOMIC_GOOD_CASE
                          ${_extra})
        if(NOT _ok)
            message(FATAL_ERROR
                "LUMEX_ATOMIC_GOOD_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
        endif()

        set(_cases 1 2 3 4)
        _lumex_atomic_try(_has_diagnose_if probe_${_tag} ${_std}
                          -DLUMEX_ATOMIC_PROBE_DIAGNOSE_IF ${_extra})
        if(_has_diagnose_if)
            foreach(_case RANGE 5 26)
                list(APPEND _cases ${_case})
            endforeach()
        else()
            message(STATUS "${_tag}: no diagnose_if attribute; memory order "
                           "cases 5-26 skipped")
        endif()

        foreach(_case IN LISTS _cases)
            _lumex_atomic_try(_ok bad_${_case}_${_tag} ${_std}
                              -DLUMEX_ATOMIC_BAD_CASE=${_case} ${_extra})
            if(_ok)
                message(FATAL_ERROR
                    "LUMEX_ATOMIC_BAD_CASE=${_case} (${_tag}) compiled; the "
                    "check that should reject it is missing")
            endif()
            math(EXPR _checked "${_checked} + 1")
        endforeach()
    endforeach()
endforeach()

message(STATUS "atomic compile checks: ${_checked} rejected cases as expected")
