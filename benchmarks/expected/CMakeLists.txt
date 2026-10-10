# benchmarks/expected/CMakeLists.txt
#
# The expected of this library against the std::expected of the standard
# library in use and boost::outcome_v2::result of Boost.Outcome. Built only
# with LUMEX_BUILD_BENCHMARKS=ON (OFF by default); a Release build without
# sanitizers is the only one whose numbers mean anything.
#
# One source (bench_expected.cpp), one executable per implementation and C++
# standard, all with the flags of the Release profile:
#
#   LumexExpectedBench_lumex_cxx11  the expected of this library at C++11
#   LumexExpectedBench_lumex_cxx14  the same at C++14
#   LumexExpectedBench_lumex_cxx17  the same at C++17
#   LumexExpectedBench_lumex_cxx20  the same at C++20
#   LumexExpectedBench_lumex_cxx23  the same at C++23
#   LumexExpectedBench_std_cxx23    std::expected of the standard library in
#                                   use (only where <expected> exists)
#   LumexExpectedBench_outcome_cxx17  boost::outcome_v2::result at C++17
#                                   (only where Boost.Outcome is found)
#
# Boost.Outcome is optional and header-only (it is in Boost 1.70 and later).
# It is found by
#   -DLUMEX_BENCH_BOOST_INCLUDE_DIR=<dir that holds boost/outcome/result.hpp>
# or by find_package(Boost CONFIG) (point CMAKE_PREFIX_PATH or Boost_DIR at the
# installation). Without it the outcome variant is skipped. Nothing is
# vendored and nothing is linked. tl::expected is not looked for: it is not
# installed on the machine of the committed results.
#
# A standard library needs its own compiler, so the comparison of two
# toolchains is two build trees whose bin/ directories run_benchmark.py reads
# together:
#
#   cmake -S . -B build-bench-gcc -DCMAKE_BUILD_TYPE=Release
#         -DCMAKE_CXX_COMPILER=g++ -DLUMEX_BUILD_BENCHMARKS=ON
#         -DLUMEX_BENCH_BOOST_INCLUDE_DIR=/opt/boost-1.92.0/include
#   cmake --build build-bench-gcc
#   python benchmarks/expected/run_benchmark.py --build-dir gcc=build-bench-gcc

set(LUMEX_BENCH_BOOST_INCLUDE_DIR "" CACHE PATH
    "Directory that holds boost/outcome/result.hpp (headers only); empty: find_package(Boost)")

set(_lumex_bench_boost_include "")
if(LUMEX_BENCH_BOOST_INCLUDE_DIR
   AND EXISTS "${LUMEX_BENCH_BOOST_INCLUDE_DIR}/boost/outcome/result.hpp")
    set(_lumex_bench_boost_include "${LUMEX_BENCH_BOOST_INCLUDE_DIR}")
else()
    find_package(Boost QUIET CONFIG)
    if(TARGET Boost::headers)
        get_target_property(_lumex_bench_boost_dirs Boost::headers
            INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_dir IN LISTS _lumex_bench_boost_dirs)
            if(EXISTS "${_dir}/boost/outcome/result.hpp")
                set(_lumex_bench_boost_include "${_dir}")
                break()
            endif()
        endforeach()
    endif()
endif()
if(_lumex_bench_boost_include)
    message(STATUS "expected benchmark: Boost.Outcome from ${_lumex_bench_boost_include}")
else()
    message(STATUS "expected benchmark: Boost.Outcome not found "
        "(set LUMEX_BENCH_BOOST_INCLUDE_DIR); the outcome variant is skipped")
endif()

# Does the standard library have std::expected at C++23?
include(CheckCXXSourceCompiles)
set(_lumex_bench_saved_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
check_cxx_source_compiles("
#include <expected>
#if !defined(__cpp_lib_expected)
#error no std::expected
#endif
int main () { std::expected<int, int> e (1); return e.value () - 1; }
" LUMEX_BENCH_HAS_STD_EXPECTED)
set(CMAKE_CXX_STANDARD "${_lumex_bench_saved_standard}")

function(lumex_expected_bench_variant impl standard)
    if(impl STREQUAL "lumex")
        set(_id 1)
    elseif(impl STREQUAL "std")
        set(_id 2)
    else()
        set(_id 3)
    endif()
    if(standard GREATER 11 AND NOT "cxx_std_${standard}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        message(STATUS "expected benchmark: ${impl} at C++${standard} skipped (the compiler lacks it)")
        return()
    endif()
    set(_target LumexExpectedBench_${impl}_cxx${standard})
    add_executable(${_target} bench_expected.cpp)
    target_compile_definitions(${_target} PRIVATE BENCH_EXPECTED_IMPL=${_id})
    target_link_libraries(${_target} PRIVATE lumex::expected)
    if(impl STREQUAL "outcome")
        target_include_directories(${_target} SYSTEM PRIVATE
            "${_lumex_bench_boost_include}")
    endif()
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${standard}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    set(LUMEX_EXPECTED_BENCH_TARGETS ${LUMEX_EXPECTED_BENCH_TARGETS} ${_target}
        PARENT_SCOPE)
endfunction()

set(LUMEX_EXPECTED_BENCH_TARGETS "")
foreach(_standard 11 14 17 20 23)
    lumex_expected_bench_variant(lumex ${_standard})
endforeach()
if(LUMEX_BENCH_HAS_STD_EXPECTED)
    lumex_expected_bench_variant(std 23)
else()
    message(STATUS "expected benchmark: std::expected not available; the std variant is skipped")
endif()
if(_lumex_bench_boost_include)
    lumex_expected_bench_variant(outcome 17)
endif()

# `LumexExpectedBenchmarkRun` runs the variants of THIS build tree and writes
# the CSV, the Markdown table and the charts into the build tree; the
# committed results come from run_benchmark.py with the options of the README.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND LUMEX_EXPECTED_BENCH_TARGETS)
    add_custom_target(LumexExpectedBenchmarkRun
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
            --build-dir "this=${CMAKE_BINARY_DIR}"
            --out-dir "${CMAKE_BINARY_DIR}/expected_benchmark"
        DEPENDS ${LUMEX_EXPECTED_BENCH_TARGETS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the expected benchmark (this build tree)"
        VERBATIM
        USES_TERMINAL
    )
endif()
