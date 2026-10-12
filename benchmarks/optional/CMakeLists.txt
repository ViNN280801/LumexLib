# benchmarks/optional/CMakeLists.txt
#
# The optional of this library against the std::optional of the standard
# library in use and boost::optional of Boost.Optional. Built only with
# LUMEX_BUILD_BENCHMARKS=ON (OFF by default); a Release build without
# sanitizers is the only one whose numbers mean anything.
#
# One source (bench_optional.cpp), one executable per implementation and C++
# standard, all with the flags of the Release profile:
#
#   LumexOptionalBench_lumex_cxx11   the optional of this library at C++11
#   LumexOptionalBench_lumex_cxx17   the same at C++17 (it does not become
#                                    std::optional there)
#   LumexOptionalBench_std_cxx17     std::optional of the standard library in
#                                    use (only where <optional> exists)
#   LumexOptionalBench_boost_cxx11   boost::optional at C++11
#   LumexOptionalBench_boost_cxx17   boost::optional at C++17
#                                    (only where Boost.Optional is found)
#
# Boost is optional and header-only. It is found by
#   -DLUMEX_BENCH_BOOST_INCLUDE_DIR=<dir that holds boost/optional.hpp>
# or by find_package(Boost CONFIG) (point CMAKE_PREFIX_PATH or Boost_DIR at the
# installation). Without it the boost variants are skipped. Nothing of Boost is
# linked or vendored.
#
# A standard library needs its own compiler, so the comparison of two
# toolchains is two build trees whose bin/ directories run_benchmark.py reads
# together:
#
#   cmake -S . -B build-bench-gcc -DCMAKE_BUILD_TYPE=Release
#         -DCMAKE_CXX_COMPILER=g++ -DLUMEX_BUILD_BENCHMARKS=ON
#         -DLUMEX_BENCH_BOOST_INCLUDE_DIR=/opt/boost-1.92.0/include
#   cmake --build build-bench-gcc --target LumexOptionalBenchmarkRun

set(LUMEX_BENCH_BOOST_INCLUDE_DIR "" CACHE PATH
    "Directory that holds boost/optional.hpp (headers only); empty: find_package(Boost)")

set(_lumex_bench_boost_include "")
if(LUMEX_BENCH_BOOST_INCLUDE_DIR
   AND EXISTS "${LUMEX_BENCH_BOOST_INCLUDE_DIR}/boost/optional.hpp")
    set(_lumex_bench_boost_include "${LUMEX_BENCH_BOOST_INCLUDE_DIR}")
else()
    find_package(Boost QUIET CONFIG)
    if(TARGET Boost::headers)
        get_target_property(_lumex_bench_boost_dirs Boost::headers
            INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_dir IN LISTS _lumex_bench_boost_dirs)
            if(EXISTS "${_dir}/boost/optional.hpp")
                set(_lumex_bench_boost_include "${_dir}")
                break()
            endif()
        endforeach()
    endif()
endif()
if(_lumex_bench_boost_include)
    message(STATUS "optional benchmark: boost::optional from ${_lumex_bench_boost_include}")
else()
    message(STATUS "optional benchmark: Boost.Optional not found "
        "(set LUMEX_BENCH_BOOST_INCLUDE_DIR); the boost variants are skipped")
endif()

# Does the standard library have std::optional at C++17?
include(CheckCXXSourceCompiles)
set(_lumex_bench_saved_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
check_cxx_source_compiles("
#include <optional>
int main () { std::optional<int> o (1); return *o - 1; }
" LUMEX_BENCH_HAS_STD_OPTIONAL)
set(CMAKE_CXX_STANDARD "${_lumex_bench_saved_standard}")

function(lumex_optional_bench_variant impl standard)
    if(impl STREQUAL "lumex")
        set(_id 1)
    elseif(impl STREQUAL "std")
        set(_id 2)
    else()
        set(_id 3)
    endif()
    if(standard GREATER 11 AND NOT "cxx_std_${standard}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        message(STATUS "optional benchmark: ${impl} at C++${standard} skipped (the compiler lacks it)")
        return()
    endif()
    set(_target LumexOptionalBench_${impl}_cxx${standard})
    add_executable(${_target} bench_optional.cpp)
    target_compile_definitions(${_target} PRIVATE BENCH_OPTIONAL_IMPL=${_id})
    target_link_libraries(${_target} PRIVATE lumex::optional)
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
    set(LUMEX_OPTIONAL_BENCH_TARGETS ${LUMEX_OPTIONAL_BENCH_TARGETS} ${_target}
        PARENT_SCOPE)
endfunction()

set(LUMEX_OPTIONAL_BENCH_TARGETS "")
lumex_optional_bench_variant(lumex 11)
lumex_optional_bench_variant(lumex 17)
if(LUMEX_BENCH_HAS_STD_OPTIONAL)
    lumex_optional_bench_variant(std 17)
else()
    message(STATUS "optional benchmark: std::optional not available; the std variant is skipped")
endif()
if(_lumex_bench_boost_include)
    lumex_optional_bench_variant(boost 11)
    lumex_optional_bench_variant(boost 17)
endif()

# `LumexOptionalBenchmarkRun` runs the variants of THIS build tree and writes
# the CSV, the Markdown table and the charts into the build tree; the
# committed results come from run_benchmark.py (see README.md).
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND LUMEX_OPTIONAL_BENCH_TARGETS)
    add_custom_target(LumexOptionalBenchmarkRun
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
            --build-dir "this=${CMAKE_BINARY_DIR}"
            --out-dir "${CMAKE_BINARY_DIR}/optional_benchmark"
        DEPENDS ${LUMEX_OPTIONAL_BENCH_TARGETS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the optional benchmark (this build tree)"
        VERBATIM
        USES_TERMINAL
    )
endif()
