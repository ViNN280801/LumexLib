# Compile-check fixture for cmake.atomic_compile_checks.
#
# Every LUMEX_ATOMIC_BAD_CASE in check.cpp must fail to compile with warnings
# as errors, and LUMEX_ATOMIC_GOOD_CASE must compile. Cases 1-4 discard the
# result of load () or is_lock_free () (nodiscard); cases 5-26 pass a constant
# memory order that the standard forbids, which only a compiler with the
# diagnose_if attribute (Clang) reports, so they are skipped elsewhere. The
# checks run at C++11 and C++20, and at C++20 with the forced lock-based
# implementation (MSVC: C++20 only; its C++14 nodiscard needs /analyze).
# The same checks run again with the lock-free engine present (the detection
# macro LUMEX_ATOMIC_HAS_HAZARD_POINTER defined by hand), and a second group
# checks the engine names: the lock-free classes compile where the engine
# exists and are a compile error where it does not (no definition, or
# LUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE), the common names are the
# lock-free or the lock-based engine accordingly, and the wrapper of the
# standard library's type cannot be named below C++20.
# Everything runs at configure time (try_compile; the sources are compiled,
# not linked, because the lock-free engine needs the hazard pointer library).

cmake_minimum_required(VERSION 3.16)
project(LumexAtomicCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

find_package(Threads REQUIRED)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

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
foreach(_variant IN ITEMS "plain" "forced" "free")
    foreach(_std IN LISTS _standards)
        if(_variant STREQUAL "forced")
            if(NOT _std EQUAL 20)
                continue()
            endif()
            set(_extra -DLUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED)
        elseif(_variant STREQUAL "free")
            set(_extra -DLUMEX_ATOMIC_HAS_HAZARD_POINTER=1)
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

# --- the engine names --------------------------------------------------------

foreach(_std IN LISTS _standards)
    set(_tag "names_cxx${_std}")
    # With the engine: the lock-free names compile, the common names are the
    # lock-free engine.
    _lumex_atomic_try(_ok good_free_${_tag} ${_std} -DLUMEX_ATOMIC_NAME_GOOD_FREE
                      -DLUMEX_ATOMIC_HAS_HAZARD_POINTER=1)
    if(NOT _ok)
        message(FATAL_ERROR
            "LUMEX_ATOMIC_NAME_GOOD_FREE (${_tag}) must compile\n${_ok_OUTPUT}")
    endif()
    # Without the detection macro, and with the engine switched off: the
    # common names are the lock-based engine and the lock-free names do not
    # exist.
    foreach(_off IN ITEMS "no_macro" "disabled")
        if(_off STREQUAL "disabled")
            set(_off_args -DLUMEX_ATOMIC_HAS_HAZARD_POINTER=1
                          -DLUMEX_ATOMIC_SMART_PTR_DISABLE_LOCK_FREE)
        else()
            set(_off_args "")
        endif()
        _lumex_atomic_try(_ok good_lb_${_tag}_${_off} ${_std}
                          -DLUMEX_ATOMIC_NAME_GOOD_LOCK_BASED ${_off_args})
        if(NOT _ok)
            message(FATAL_ERROR
                "LUMEX_ATOMIC_NAME_GOOD_LOCK_BASED (${_tag}, ${_off}) must "
                "compile\n${_ok_OUTPUT}")
        endif()
        foreach(_bad IN ITEMS LUMEX_ATOMIC_NAME_BAD_FREE
                              LUMEX_ATOMIC_NAME_BAD_FREE_WEAK)
            _lumex_atomic_try(_ok bad_${_bad}_${_tag}_${_off} ${_std}
                              -D${_bad} ${_off_args})
            if(_ok)
                message(FATAL_ERROR
                    "${_bad} (${_tag}, ${_off}) compiled: naming the lock-free "
                    "engine must fail where it does not exist")
            endif()
            math(EXPR _checked "${_checked} + 1")
        endforeach()
    endforeach()
endforeach()
# The wrapper of the standard library's type cannot be named at C++11.
_lumex_atomic_try(_ok bad_std_backed_cxx11 11 -DLUMEX_ATOMIC_NAME_BAD_STD_BACKED)
if(_ok)
    message(FATAL_ERROR
        "LUMEX_ATOMIC_NAME_BAD_STD_BACKED compiled at C++11")
endif()
math(EXPR _checked "${_checked} + 1")

message(STATUS "atomic compile checks: ${_checked} rejected cases as expected")
