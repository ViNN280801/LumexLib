# benchmarks/string_view/CMakeLists.txt
#
# The string view of this library (lumex_string_view) against the
# std::string_view of the standard library in use and boost::string_view of
# Boost.Utility. Built only with LUMEX_BUILD_BENCHMARKS=ON (OFF by default); a
# Release build without sanitizers is the only one whose numbers mean anything.
#
# One source (bench_string_view.cpp), one executable per implementation and C++
# standard, all with the flags of the Release profile:
#
#   LumexStringViewBench_lumex_cxx11   the view of this library at C++11
#   LumexStringViewBench_lumex_cxx17   the same at C++17 (it does not become
#                                      std::string_view there)
#   LumexStringViewBench_lumex_unity_cxx17
#                                      the view of this library with its
#                                      compiled part (the .cpp files) built
#                                      into the benchmark's translation unit,
#                                      so that it is inlined and not linked
#   LumexStringViewBench_std_cxx17     std::string_view of the standard library
#                                      in use (only where <string_view> exists)
#   LumexStringViewBench_std_cxx20     the same at C++20, where starts_with and
#                                      ends_with exist
#   LumexStringViewBench_boost_cxx11   boost::string_view at C++11
#   LumexStringViewBench_boost_cxx17   boost::string_view at C++17
#                                      (only where Boost.Utility is found)
#
# Boost is optional and header-only. It is found by
#   -DLUMEX_BENCH_BOOST_INCLUDE_DIR=<dir that holds boost/utility/string_view.hpp>
# or by find_package(Boost CONFIG) (point CMAKE_PREFIX_PATH or Boost_DIR at the
# installation). Without it the boost variants are skipped. Nothing of Boost is
# linked or vendored.
#
# The view has a compiled part (lumex::string_view): whether it is a shared or
# a static library follows LUMEX_BUILD_SHARED_LIBS, so the comparison of the
# two is two build trees whose bin/ directories run_benchmark.py reads
# together (a standard library needs its own compiler for the same reason):
#
#   cmake -S . -B build-bench-shared -DCMAKE_BUILD_TYPE=Release
#         -DCMAKE_CXX_COMPILER=g++ -DLUMEX_BUILD_BENCHMARKS=ON
#         -DLUMEX_BENCH_BOOST_INCLUDE_DIR=/opt/boost-1.92.0/include
#   cmake -S . -B build-bench-static -DCMAKE_BUILD_TYPE=Release
#         -DCMAKE_CXX_COMPILER=g++ -DLUMEX_BUILD_BENCHMARKS=ON
#         -DLUMEX_BUILD_SHARED_LIBS=OFF
#         -DLUMEX_BENCH_BOOST_INCLUDE_DIR=/opt/boost-1.92.0/include
#   cmake --build build-bench-shared --target LumexStringViewBenchmarkRun

set(LUMEX_BENCH_BOOST_INCLUDE_DIR "" CACHE PATH
    "Directory that holds boost/utility/string_view.hpp (headers only); empty: find_package(Boost)")

set(_lumex_bench_boost_include "")
if(LUMEX_BENCH_BOOST_INCLUDE_DIR
   AND EXISTS "${LUMEX_BENCH_BOOST_INCLUDE_DIR}/boost/utility/string_view.hpp")
    set(_lumex_bench_boost_include "${LUMEX_BENCH_BOOST_INCLUDE_DIR}")
else()
    find_package(Boost QUIET CONFIG)
    if(TARGET Boost::headers)
        get_target_property(_lumex_bench_boost_dirs Boost::headers
            INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_dir IN LISTS _lumex_bench_boost_dirs)
            if(EXISTS "${_dir}/boost/utility/string_view.hpp")
                set(_lumex_bench_boost_include "${_dir}")
                break()
            endif()
        endforeach()
    endif()
endif()
if(_lumex_bench_boost_include)
    message(STATUS "string_view benchmark: boost::string_view from ${_lumex_bench_boost_include}")
else()
    message(STATUS "string_view benchmark: Boost.Utility not found "
        "(set LUMEX_BENCH_BOOST_INCLUDE_DIR); the boost variants are skipped")
endif()

# Does the standard library have std::string_view at C++17?
include(CheckCXXSourceCompiles)
set(_lumex_bench_saved_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
check_cxx_source_compiles("
#include <string_view>
int main () { std::string_view v (\"a\"); return static_cast<int> (v.size ()) - 1; }
" LUMEX_BENCH_HAS_STD_STRING_VIEW)
set(CMAKE_CXX_STANDARD "${_lumex_bench_saved_standard}")

function(lumex_string_view_bench_variant impl standard)
    if(impl STREQUAL "lumex" OR impl STREQUAL "lumex_unity")
        set(_id 1)
    elseif(impl STREQUAL "std")
        set(_id 2)
    else()
        set(_id 3)
    endif()
    if(standard GREATER 11 AND NOT "cxx_std_${standard}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        message(STATUS "string_view benchmark: ${impl} at C++${standard} skipped (the compiler lacks it)")
        return()
    endif()
    set(_target LumexStringViewBench_${impl}_cxx${standard})
    add_executable(${_target} bench_string_view.cpp)
    target_compile_definitions(${_target} PRIVATE BENCH_STRING_VIEW_IMPL=${_id})
    if(impl STREQUAL "lumex_unity")
        target_compile_definitions(${_target} PRIVATE BENCH_LUMEX_UNITY=1)
        target_link_libraries(${_target} PRIVATE lumex::utility)
        target_include_directories(${_target} PRIVATE "${LumexLib_SOURCE_DIR}")
    else()
        target_link_libraries(${_target} PRIVATE lumex::string_view)
    endif()
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
    set(LUMEX_STRING_VIEW_BENCH_TARGETS ${LUMEX_STRING_VIEW_BENCH_TARGETS} ${_target}
        PARENT_SCOPE)
endfunction()

set(LUMEX_STRING_VIEW_BENCH_TARGETS "")
lumex_string_view_bench_variant(lumex 11)
lumex_string_view_bench_variant(lumex 17)
lumex_string_view_bench_variant(lumex_unity 17)
if(LUMEX_BENCH_HAS_STD_STRING_VIEW)
    lumex_string_view_bench_variant(std 17)
    lumex_string_view_bench_variant(std 20)
else()
    message(STATUS "string_view benchmark: std::string_view not available; the std variants are skipped")
endif()
if(_lumex_bench_boost_include)
    lumex_string_view_bench_variant(boost 11)
    lumex_string_view_bench_variant(boost 17)
endif()

# `LumexStringViewBenchmarkRun` runs the variants of THIS build tree and writes
# the CSV, the Markdown table and the charts into the build tree; the
# committed results come from run_benchmark.py (see README.md).
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND LUMEX_STRING_VIEW_BENCH_TARGETS)
    add_custom_target(LumexStringViewBenchmarkRun
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
            --build-dir "this=${CMAKE_BINARY_DIR}"
            --out-dir "${CMAKE_BINARY_DIR}/string_view_benchmark"
        DEPENDS ${LUMEX_STRING_VIEW_BENCH_TARGETS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the string_view benchmark (this build tree)"
        VERBATIM
        USES_TERMINAL
    )
endif()
