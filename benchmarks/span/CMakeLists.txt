# benchmarks/span/CMakeLists.txt
#
# The span of this library against the std::span of the standard library in
# use and boost::span of Boost.Core. Built only with LUMEX_BUILD_BENCHMARKS=ON
# (OFF by default); a Release build without sanitizers is the only one whose
# numbers mean anything.
#
# One source (bench_span.cpp), one executable per implementation and C++
# standard, all with the flags of the Release profile:
#
#   LumexSpanBench_lumex_cxx11   the span of this library at C++11
#   LumexSpanBench_lumex_cxx20   the same at C++20
#   LumexSpanBench_std_cxx20     std::span of the standard library in use
#                                (only where <span> exists)
#   LumexSpanBench_boost_cxx11   boost::span at C++11
#   LumexSpanBench_boost_cxx20   boost::span at C++20
#                                (only where Boost.Core 1.78 or later is found)
#
# Boost is optional and header-only. It is found by
#   -DLUMEX_BENCH_BOOST_INCLUDE_DIR=<dir that holds boost/core/span.hpp>
# or by find_package(Boost 1.78 CONFIG) (point CMAKE_PREFIX_PATH or Boost_DIR
# at the installation). Without it the boost variants are skipped.
#
# A standard library needs its own compiler, so the comparison of two
# toolchains is two build trees whose bin/ directories run_benchmark.py reads
# together:
#
#   cmake -S . -B build-bench-gcc -DCMAKE_BUILD_TYPE=Release
#         -DCMAKE_CXX_COMPILER=g++ -DLUMEX_BUILD_BENCHMARKS=ON
#         -DLUMEX_BENCH_BOOST_INCLUDE_DIR=/opt/boost-1.92.0/include
#   cmake --build build-bench-gcc
#   (the same with clang++ in build-bench-clang)
#   python benchmarks/span/run_benchmark.py
#          --build-dir gcc=build-bench-gcc --build-dir clang=build-bench-clang

set(LUMEX_BENCH_BOOST_INCLUDE_DIR "" CACHE PATH
    "Directory that holds boost/core/span.hpp (Boost 1.78 or later, headers only); empty: find_package(Boost)")

set(_lumex_bench_boost_include "")
if(LUMEX_BENCH_BOOST_INCLUDE_DIR
   AND EXISTS "${LUMEX_BENCH_BOOST_INCLUDE_DIR}/boost/core/span.hpp")
    set(_lumex_bench_boost_include "${LUMEX_BENCH_BOOST_INCLUDE_DIR}")
else()
    find_package(Boost 1.78 QUIET CONFIG)
    if(TARGET Boost::headers)
        get_target_property(_lumex_bench_boost_dirs Boost::headers
            INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_dir IN LISTS _lumex_bench_boost_dirs)
            if(EXISTS "${_dir}/boost/core/span.hpp")
                set(_lumex_bench_boost_include "${_dir}")
                break()
            endif()
        endforeach()
    endif()
endif()
if(_lumex_bench_boost_include)
    message(STATUS "span benchmark: boost::span from ${_lumex_bench_boost_include}")
else()
    message(STATUS "span benchmark: Boost.Core 1.78 or later not found "
        "(set LUMEX_BENCH_BOOST_INCLUDE_DIR); the boost variants are skipped")
endif()

# Does the standard library have std::span at C++20?
include(CheckCXXSourceCompiles)
set(_lumex_bench_saved_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
check_cxx_source_compiles("
#include <span>
#if !defined(__cpp_lib_span)
#error no std::span
#endif
int main () { int a[2] = { 1, 2 }; std::span<int> s (a); return static_cast<int> (s.size ()) - 2; }
" LUMEX_BENCH_HAS_STD_SPAN)
set(CMAKE_CXX_STANDARD "${_lumex_bench_saved_standard}")

function(lumex_span_bench_variant impl standard)
    if(impl STREQUAL "lumex")
        set(_id 1)
    elseif(impl STREQUAL "std")
        set(_id 2)
    else()
        set(_id 3)
    endif()
    if(standard GREATER 11 AND NOT "cxx_std_${standard}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        message(STATUS "span benchmark: ${impl} at C++${standard} skipped (the compiler lacks it)")
        return()
    endif()
    set(_target LumexSpanBench_${impl}_cxx${standard})
    add_executable(${_target} bench_span.cpp)
    target_compile_definitions(${_target} PRIVATE BENCH_SPAN_IMPL=${_id})
    target_link_libraries(${_target} PRIVATE lumex::span)
    if(impl STREQUAL "boost")
        target_include_directories(${_target} SYSTEM PRIVATE
            "${_lumex_bench_boost_include}")
    endif()
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${standard}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    if(MSVC)
        target_compile_options(${_target} PRIVATE /utf-8)
    endif()
    set(LUMEX_SPAN_BENCH_TARGETS ${LUMEX_SPAN_BENCH_TARGETS} ${_target}
        PARENT_SCOPE)
endfunction()

set(LUMEX_SPAN_BENCH_TARGETS "")
lumex_span_bench_variant(lumex 11)
lumex_span_bench_variant(lumex 20)
if(LUMEX_BENCH_HAS_STD_SPAN)
    lumex_span_bench_variant(std 20)
else()
    message(STATUS "span benchmark: std::span not available; the std variant is skipped")
endif()
if(_lumex_bench_boost_include)
    lumex_span_bench_variant(boost 11)
    lumex_span_bench_variant(boost 20)
endif()

# `LumexSpanBenchmarkRun` runs the variants of THIS build tree and writes the
# CSV, the Markdown table and the charts into the build tree; the committed
# results (two toolchains) come from run_benchmark.py with both build trees.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND LUMEX_SPAN_BENCH_TARGETS)
    add_custom_target(LumexSpanBenchmarkRun
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
            --build-dir "this=${CMAKE_BINARY_DIR}"
            --out-dir "${CMAKE_BINARY_DIR}/span_benchmark"
        DEPENDS ${LUMEX_SPAN_BENCH_TARGETS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the span benchmark (this build tree)"
        VERBATIM
        USES_TERMINAL
    )
endif()
