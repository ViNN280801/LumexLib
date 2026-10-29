# Compile-check fixture for cmake.timing_compile_checks.
#
# The timer seam of the transport tells time in time points of its own clock
# (timing::timer_clock), so that a time point of std::chrono::steady_clock
# does not mix with it, and device_watcher has one constructor (with
# device_watcher_options_t). Every LUMEX_TIMING_BAD_CASE in check.cpp must
# fail to compile, naming the type it mixes up, and LUMEX_TIMING_GOOD_CASE
# must compile:
#   1 async_wait_until with a steady_clock time point
#   2 a time_point_t initialized from steady_clock::now ()
#   3 a virtual_clock started at a steady_clock time point
#   4 a time of the seam compared with a steady_clock time point
#   5 device_watcher (executor, period)
#   6 device_watcher (executor, period, enumeration)
# The headers are compiled without the library. Everything runs at configure
# time (try_compile), at C++11 (MSVC: C++14, its lowest) and C++20.

cmake_minimum_required(VERSION 3.16)
project(LumexTimingCompileChecks LANGUAGES CXX)

if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLUMEX_SOURCE_DIR=<LumexLib checkout>")
endif()

set(ENV{LC_ALL} C)
set(ENV{LANGUAGE} C)

set(_source "${CMAKE_CURRENT_SOURCE_DIR}/check.cpp")
set(_includes "${LUMEX_SOURCE_DIR}" "${LUMEX_SOURCE_DIR}/3rdparty/asio/include")
set(_defines -DASIO_STANDALONE -DASIO_NO_DEPRECATED)
if(MSVC)
    set(_werror /WX /Zc:__cplusplus /permissive- /EHsc)
    set(_all_standards 14 20)
    list(APPEND _defines -D_WIN32_WINNT=0x0A00)
else()
    set(_werror -Werror)
    set(_all_standards 11 20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # A --gcc-toolchain flag of the calling tree is unused when compiling.
        list(APPEND _werror -Wno-unused-command-line-argument)
    endif()
endif()

# The files are compiled, not run: a static library needs no main.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_steady_regex "steady_clock")
set(_watcher_regex "device_watcher")
set(_cases 1 2 3 4 5 6)
set(_case_regex_1 "${_steady_regex}")
set(_case_regex_2 "${_steady_regex}")
set(_case_regex_3 "${_steady_regex}")
set(_case_regex_4 "${_steady_regex}")
set(_case_regex_5 "${_watcher_regex}")
set(_case_regex_6 "${_watcher_regex}")

function(_lumex_timing_try result_var tag standard)
    try_compile(_ok "${CMAKE_BINARY_DIR}/try_${tag}"
        SOURCES "${_source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${_includes}"
        COMPILE_DEFINITIONS ${_werror} ${_defines} ${ARGN}
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
    _lumex_timing_try(_good good_${_tag} ${_std} -DLUMEX_TIMING_GOOD_CASE)
    if(NOT _good)
        message(FATAL_ERROR
            "LUMEX_TIMING_GOOD_CASE (${_tag}) must compile\n${_good_OUTPUT}")
    endif()

    foreach(_case IN LISTS _cases)
        _lumex_timing_try(_bad bad_${_case}_${_tag} ${_std}
                          -DLUMEX_TIMING_BAD_CASE=${_case})
        if(_bad)
            message(FATAL_ERROR
                "LUMEX_TIMING_BAD_CASE=${_case} (${_tag}) compiled; the "
                "seam accepts what it must refuse")
        endif()
        # The case must fail on the type it mixes up, not because check.cpp
        # has a typo.
        if(NOT _bad_OUTPUT MATCHES "${_case_regex_${_case}}")
            message(FATAL_ERROR
                "LUMEX_TIMING_BAD_CASE=${_case} (${_tag}) failed, but "
                "not on /${_case_regex_${_case}}/\n${_bad_OUTPUT}")
        endif()
        math(EXPR _checked "${_checked} + 1")
    endforeach()
endforeach()

message(STATUS "timing compile checks: ${_checked} rejected cases as expected")
