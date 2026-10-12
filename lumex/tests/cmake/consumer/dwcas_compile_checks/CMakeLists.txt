# Compile-check fixture for cmake.dwcas_compile_checks.
#
# check.cpp, built with warnings as errors:
#   - LUMEX_DWCAS_GOOD_CASE must compile (every member, every order);
#   - every LUMEX_DWCAS_BAD_CASE must fail: a discarded result of load (),
#     exchange () or compare_exchange_* (nodiscard), a copy or an assignment
#     of the word, a number or a mismatched type where a value is wanted, a
#     store through a const word, a packed layout around the word;
#   - with LUMEX_ATOMIC_DISABLE_DWCAS the layer is not declared:
#     LUMEX_DWCAS_GOOD_DISABLED_CASE compiles (the macros say so and the
#     umbrella still works), every LUMEX_DWCAS_BAD_DISABLED_CASE names
#     something the layer would have declared and must fail.
# config_only.cpp includes nothing but the configuration header and compares
# LUMEX_ATOMIC_HAS_DWCAS and LUMEX_DWCAS_BACKEND with the expected values
# while the compiler's predefined macros are replaced (-undef plus chosen
# -D...), which tests the platform branches that this machine cannot run:
# AArch64, the MSVC targets, 32-bit, an unknown compiler; and the rejected
# forced backends. Those cases need a GNU-style driver (-undef) and are
# skipped on MSVC.
# The checks run at C++11 and C++20 (MSVC: C++20 only; its C++14 nodiscard
# needs /analyze). Everything runs at configure time (try_compile).

cmake_minimum_required(VERSION 3.16)
project(LumexDwcasCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

find_package(Threads REQUIRED)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
set(_config_source "${CMAKE_CURRENT_SOURCE_DIR}/config_only.cpp")
if(MSVC)
    set(_werror /WX /W4 /Zc:__cplusplus /permissive- /EHsc)
    set(_standards 20)
else()
    set(_werror -Werror -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
        -Wshadow -Wold-style-cast)
    set(_standards 11 20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # --gcc-toolchain (CMAKE_CXX_FLAGS of the libstdc++ trees) is unused
        # by a compile-only step and would fail every case under -Werror.
        list(APPEND _werror -Wno-unused-command-line-argument)
    endif()
endif()

function(_lumex_dwcas_try result_var tag standard)
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

# The layer is x86-64 only: on another host there is nothing to compile and
# the fixture only checks the configuration header.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
#if !(defined(__x86_64__) || defined(__amd64__) || defined(_M_X64))
#error not x86-64
#endif
int main () { return 0; }" LUMEX_DWCAS_FIXTURE_X86_64)

set(_checked 0)
if(LUMEX_DWCAS_FIXTURE_X86_64)
    foreach(_std IN LISTS _standards)
        set(_tag "cxx${_std}")
        _lumex_dwcas_try(_ok good_${_tag} ${_std} -DLUMEX_DWCAS_GOOD_CASE)
        if(NOT _ok)
            message(FATAL_ERROR
                "LUMEX_DWCAS_GOOD_CASE (${_tag}) must compile\n${_ok_OUTPUT}")
        endif()
        _lumex_dwcas_try(_ok good_disabled_${_tag} ${_std}
                         -DLUMEX_DWCAS_GOOD_DISABLED_CASE
                         -DLUMEX_ATOMIC_DISABLE_DWCAS)
        if(NOT _ok)
            message(FATAL_ERROR
                "LUMEX_DWCAS_GOOD_DISABLED_CASE (${_tag}) must compile\n"
                "${_ok_OUTPUT}")
        endif()

        foreach(_case RANGE 1 18)
            _lumex_dwcas_try(_ok bad_${_case}_${_tag} ${_std}
                             -DLUMEX_DWCAS_BAD_CASE=${_case})
            if(_ok)
                message(FATAL_ERROR
                    "LUMEX_DWCAS_BAD_CASE=${_case} (${_tag}) compiled; the "
                    "check that should reject it is missing")
            endif()
            math(EXPR _checked "${_checked} + 1")
        endforeach()
        foreach(_case RANGE 1 5)
            _lumex_dwcas_try(_ok bad_disabled_${_case}_${_tag} ${_std}
                             -DLUMEX_DWCAS_BAD_DISABLED_CASE=${_case}
                             -DLUMEX_ATOMIC_DISABLE_DWCAS)
            if(_ok)
                message(FATAL_ERROR
                    "LUMEX_DWCAS_BAD_DISABLED_CASE=${_case} (${_tag}) "
                    "compiled with LUMEX_ATOMIC_DISABLE_DWCAS; the layer "
                    "must not be declared")
            endif()
            math(EXPR _checked "${_checked} + 1")
        endforeach()
    endforeach()
else()
    message(STATUS "not an x86-64 compiler: the layer is not declared here; "
                   "only the configuration cases run")
endif()

# --- The configuration header with simulated predefined macros --------------
if(NOT MSVC)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(_config_extra "")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(_config_extra -Wno-unused-command-line-argument)
    endif()
    set(_gnu -D__GNUC__=8)
    # <name>|<expected HAS>|<expected BACKEND>|<flags>
    set(_config_cases
        "gcc_x86_64|1|1|-D__x86_64__ ${_gnu}"
        "clang_x86_64|1|1|-D__x86_64__ -D__clang__"
        "mingw_x86_64|1|1|-D__x86_64__ ${_gnu} -D_WIN32 -D_WIN64"
        "amd64_spelling|1|1|-D__amd64__ ${_gnu}"
        "msvc_x64|1|2|-D_M_X64 -D_MSC_VER=1930"
        "msvc_amd64_spelling|1|2|-D_M_AMD64 -D_MSC_VER=1930"
        "clang_cl_x64|1|1|-D_M_X64 -D_MSC_VER=1930 -D__clang__"
        "forced_asm|1|1|-D__x86_64__ ${_gnu} -DLUMEX_DWCAS_BACKEND=1"
        "forced_msvc_wrapper|1|2|-D__x86_64__ ${_gnu} -DLUMEX_DWCAS_BACKEND=2"
        "forced_builtin|1|3|-D__x86_64__ ${_gnu} -D__SIZEOF_INT128__=16 -DLUMEX_DWCAS_BACKEND=3"
        "aarch64|0|0|-D__aarch64__ ${_gnu}"
        "msvc_arm64|0|0|-D_M_ARM64 -D_MSC_VER=1930"
        "msvc_arm64ec|0|0|-D_M_X64 -D_M_ARM64EC -D_MSC_VER=1930"
        "i386|0|0|-D__i386__ ${_gnu}"
        "msvc_x86|0|0|-D_M_IX86 -D_MSC_VER=1930"
        "x32_abi|0|0|-D__x86_64__ -D__ILP32__ ${_gnu}"
        "ppc64|0|0|-D__powerpc64__ ${_gnu}"
        "riscv64|0|0|-D__riscv ${_gnu}"
        "unknown_compiler|0|0|-D__x86_64__"
        "disabled_x86_64|0|0|-D__x86_64__ ${_gnu} -DLUMEX_ATOMIC_DISABLE_DWCAS"
        "disabled_msvc_x64|0|0|-D_M_X64 -D_MSC_VER=1930 -DLUMEX_ATOMIC_DISABLE_DWCAS"
        "forced_ignored_on_aarch64|0|0|-D__aarch64__ ${_gnu} -DLUMEX_DWCAS_BACKEND=3"
    )
    # Cases that must not compile: a backend the compiler cannot implement
    # and a value that names no backend.
    set(_config_bad_cases
        "forced_asm_without_gnu|1|-D_M_X64 -D_MSC_VER=1930 -DLUMEX_DWCAS_BACKEND=1"
        "forced_builtin_without_int128|3|-D__x86_64__ ${_gnu} -DLUMEX_DWCAS_BACKEND=3"
        "forced_builtin_without_gnu|3|-D_M_X64 -D_MSC_VER=1930 -D__SIZEOF_INT128__=16 -DLUMEX_DWCAS_BACKEND=3"
        "forced_zero|0|-D__x86_64__ ${_gnu} -DLUMEX_DWCAS_BACKEND=0"
        "forced_four|4|-D__x86_64__ ${_gnu} -DLUMEX_DWCAS_BACKEND=4"
        "forced_seven|7|-D__x86_64__ ${_gnu} -DLUMEX_DWCAS_BACKEND=7"
    )

    foreach(_entry IN LISTS _config_cases)
        string(REPLACE "|" ";" _parts "${_entry}")
        list(GET _parts 0 _name)
        list(GET _parts 1 _has)
        list(GET _parts 2 _backend)
        list(GET _parts 3 _flags)
        separate_arguments(_flag_list UNIX_COMMAND "${_flags}")
        try_compile(_ok "${CMAKE_BINARY_DIR}/config_${_name}"
            SOURCES "${_config_source}"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
            COMPILE_DEFINITIONS -undef -Werror ${_config_extra} ${_flag_list}
                -DEXPECT_HAS=${_has} -DEXPECT_BACKEND=${_backend}
            CXX_STANDARD 11
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            OUTPUT_VARIABLE _output)
        if(NOT _ok)
            message(FATAL_ERROR
                "configuration case '${_name}' must give LUMEX_ATOMIC_HAS_DWCAS "
                "${_has} and LUMEX_DWCAS_BACKEND ${_backend}\n${_output}")
        endif()
        math(EXPR _checked "${_checked} + 1")
        # The wrong expectation must be rejected, or the case proves nothing.
        math(EXPR _wrong_has "1 - ${_has}")
        try_compile(_ok "${CMAKE_BINARY_DIR}/config_wrong_${_name}"
            SOURCES "${_config_source}"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
            COMPILE_DEFINITIONS -undef -Werror ${_config_extra} ${_flag_list}
                -DEXPECT_HAS=${_wrong_has} -DEXPECT_BACKEND=${_backend}
            CXX_STANDARD 11
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF)
        if(_ok)
            message(FATAL_ERROR
                "configuration case '${_name}' accepted the wrong "
                "LUMEX_ATOMIC_HAS_DWCAS; the comparison in config_only.cpp "
                "is not checking")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()

    foreach(_entry IN LISTS _config_bad_cases)
        string(REPLACE "|" ";" _parts "${_entry}")
        list(GET _parts 0 _name)
        list(GET _parts 1 _forced)
        list(GET _parts 2 _flags)
        separate_arguments(_flag_list UNIX_COMMAND "${_flags}")
        try_compile(_ok "${CMAKE_BINARY_DIR}/config_bad_${_name}"
            SOURCES "${_config_source}"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${LUMEX_SOURCE_DIR}"
            COMPILE_DEFINITIONS -undef -Werror ${_config_extra} ${_flag_list}
                -DEXPECT_HAS=1 -DEXPECT_BACKEND=${_forced}
            CXX_STANDARD 11
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF)
        if(_ok)
            message(FATAL_ERROR
                "configuration case '${_name}' compiled; the header must "
                "reject that forced backend with #error")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
else()
    message(STATUS "MSVC driver: the configuration cases (-undef) are skipped")
endif()

message(STATUS "dwcas compile checks: ${_checked} cases as expected")
