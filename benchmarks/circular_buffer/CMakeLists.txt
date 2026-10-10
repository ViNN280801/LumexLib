# benchmarks/circular_buffer/CMakeLists.txt
#
# The circular_buffer of this library against boost::circular_buffer of
# Boost.CircularBuffer and against what a user writes without a ring library:
# std::deque used as a bounded queue and std::vector with manual index math
# (the wrap by a compare, and by `%` as the library computes it). Built only
# with LUMEX_BUILD_BENCHMARKS=ON (OFF by default); a Release build without
# sanitizers is the only one whose numbers mean anything.
#
# One source (bench_circular_buffer.cpp), one executable per implementation and
# C++ standard, all with the flags of the Release profile:
#
#   LumexCircularBufferBench_lumex_cxx11        the buffer of this library, C++11
#   LumexCircularBufferBench_lumex_cxx17        the same at C++17
#   LumexCircularBufferBench_boost_cxx11        boost::circular_buffer, C++11
#   LumexCircularBufferBench_boost_cxx17        the same at C++17 (only where
#                                               Boost.CircularBuffer is found)
#   LumexCircularBufferBench_deque_cxx17        std::deque as a bounded queue
#   LumexCircularBufferBench_vector_cxx17       std::vector ring, wrap by compare
#   LumexCircularBufferBench_vector_mod_cxx17   std::vector ring, wrap by `%`
#   LumexCircularBufferBench_lumex_wrap_cxx17   experiment: the header of this
#                                               library with the three `%` of
#                                               the index computation replaced
#                                               by a compare (generated copy)
#
# Boost is optional and header-only. It is found by
#   -DLUMEX_BENCH_BOOST_INCLUDE_DIR=<dir that holds boost/circular_buffer.hpp>
# or by find_package(Boost CONFIG) (point CMAKE_PREFIX_PATH or Boost_DIR at the
# installation). Without it the boost variants are skipped. Nothing of Boost is
# linked or vendored.
#
#   cmake -S . -B build-bench-gcc -DCMAKE_BUILD_TYPE=Release
#         -DCMAKE_CXX_COMPILER=g++ -DLUMEX_BUILD_BENCHMARKS=ON
#         -DLUMEX_BENCH_BOOST_INCLUDE_DIR=/opt/boost-1.92.0/include
#   cmake --build build-bench-gcc --target LumexCircularBufferBenchmarkRun

set(LUMEX_BENCH_BOOST_INCLUDE_DIR "" CACHE PATH
    "Directory that holds boost/circular_buffer.hpp (headers only); empty: find_package(Boost)")

set(_lumex_bench_boost_include "")
if(LUMEX_BENCH_BOOST_INCLUDE_DIR
   AND EXISTS "${LUMEX_BENCH_BOOST_INCLUDE_DIR}/boost/circular_buffer.hpp")
    set(_lumex_bench_boost_include "${LUMEX_BENCH_BOOST_INCLUDE_DIR}")
else()
    find_package(Boost QUIET CONFIG)
    if(TARGET Boost::headers)
        get_target_property(_lumex_bench_boost_dirs Boost::headers
            INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_dir IN LISTS _lumex_bench_boost_dirs)
            if(EXISTS "${_dir}/boost/circular_buffer.hpp")
                set(_lumex_bench_boost_include "${_dir}")
                break()
            endif()
        endforeach()
    endif()
endif()
if(_lumex_bench_boost_include)
    message(STATUS "circular_buffer benchmark: boost::circular_buffer from ${_lumex_bench_boost_include}")
else()
    message(STATUS "circular_buffer benchmark: Boost.CircularBuffer not found "
        "(set LUMEX_BENCH_BOOST_INCLUDE_DIR); the boost variants are skipped")
endif()

function(lumex_circular_buffer_bench_variant impl standard)
    if(impl STREQUAL "lumex")
        set(_id 1)
    elseif(impl STREQUAL "boost")
        set(_id 2)
    elseif(impl STREQUAL "deque")
        set(_id 3)
    elseif(impl STREQUAL "vector")
        set(_id 4)
    elseif(impl STREQUAL "vector_mod")
        set(_id 5)
    else()
        set(_id 6)
    endif()
    if(standard GREATER 11 AND NOT "cxx_std_${standard}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        message(STATUS "circular_buffer benchmark: ${impl} at C++${standard} skipped (the compiler lacks it)")
        return()
    endif()
    set(_target LumexCircularBufferBench_${impl}_cxx${standard})
    add_executable(${_target} bench_circular_buffer.cpp)
    target_compile_definitions(${_target} PRIVATE BENCH_RING_IMPL=${_id})
    target_link_libraries(${_target} PRIVATE lumex::circular_buffer)
    if(impl STREQUAL "lumex_wrap")
        target_include_directories(${_target} PRIVATE "${_lumex_wrap_dir}")
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
    set(LUMEX_CIRCULAR_BUFFER_BENCH_TARGETS ${LUMEX_CIRCULAR_BUFFER_BENCH_TARGETS} ${_target}
        PARENT_SCOPE)
endfunction()

# The experiment: a copy of CircularBuffer.hpp in which ONLY the three index
# computations are changed, from `% m_capacity` to a compare and a subtraction
# (the indexes are below 2 * capacity). Nothing else differs.
set(_lumex_wrap_dir "${CMAKE_CURRENT_BINARY_DIR}/compare_wrap")
file(READ "${LumexLib_SOURCE_DIR}/lumex/core/circular_buffer/CircularBuffer.hpp" _cb)
set(_cb_orig "${_cb}")
string(REPLACE "return (m_head + logical_idx) % m_capacity;"
    "size_type const p = m_head + logical_idx; return p >= m_capacity ? p - m_capacity : p;" _cb "${_cb}")
string(REPLACE "return (idx + 1) % m_capacity;"
    "return idx + 1 >= m_capacity ? idx + 1 - m_capacity : idx + 1;" _cb "${_cb}")
string(REPLACE "return (idx + m_capacity - 1) % m_capacity;"
    "return idx == 0 ? m_capacity - 1 : idx - 1;" _cb "${_cb}")
string(FIND "${_cb}" "% m_capacity" _left)
if(NOT _left EQUAL -1 OR _cb STREQUAL _cb_orig)
    message(FATAL_ERROR "circular_buffer benchmark: the compare-wrap experiment no longer matches CircularBuffer.hpp")
endif()
file(WRITE "${_lumex_wrap_dir}/circular_buffer_compare_wrap.hpp" "${_cb}")

set(LUMEX_CIRCULAR_BUFFER_BENCH_TARGETS "")
lumex_circular_buffer_bench_variant(lumex 11)
lumex_circular_buffer_bench_variant(lumex 17)
if(_lumex_bench_boost_include)
    lumex_circular_buffer_bench_variant(boost 11)
    lumex_circular_buffer_bench_variant(boost 17)
endif()
lumex_circular_buffer_bench_variant(deque 17)
lumex_circular_buffer_bench_variant(vector 17)
lumex_circular_buffer_bench_variant(vector_mod 17)
lumex_circular_buffer_bench_variant(lumex_wrap 17)

# `LumexCircularBufferBenchmarkRun` runs the variants of THIS build tree and
# writes the CSV, the Markdown table and the charts into the build tree; the
# committed results come from run_benchmark.py (see README.md).
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND LUMEX_CIRCULAR_BUFFER_BENCH_TARGETS)
    add_custom_target(LumexCircularBufferBenchmarkRun
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run_benchmark.py"
            --build-dir "this=${CMAKE_BINARY_DIR}"
            --out-dir "${CMAKE_BINARY_DIR}/circular_buffer_benchmark"
        DEPENDS ${LUMEX_CIRCULAR_BUFFER_BENCH_TARGETS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running the circular_buffer benchmark (this build tree)"
        VERBATIM
        USES_TERMINAL
    )
endif()
