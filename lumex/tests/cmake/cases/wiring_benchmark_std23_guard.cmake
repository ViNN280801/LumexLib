# benchmarks/expected asks the compiler for std::expected at C++23 in a
# check_cxx_source_compiles with CXX_STANDARD 23. CMake ends the configure
# with an error for a compiler that has no such dialect (GCC 8.3), so
# LUMEX_BUILD_BENCHMARKS=ON did not configure there. The check runs only
# when cxx_std_23 is among CMAKE_CXX_COMPILE_FEATURES (todo 181).

file(READ "${LUMEX_SOURCE_DIR}/benchmarks/expected/CMakeLists.txt" _text)

string(FIND "${_text}" "set(CMAKE_CXX_STANDARD 23)" _set)
string(FIND "${_text}" "cxx_std_23" _guard)
string(FIND "${_text}" "CMAKE_CXX_COMPILE_FEATURES" _features)
if(_set EQUAL -1)
    message(FATAL_ERROR "benchmarks/expected/CMakeLists.txt lost the C++23 check")
endif()
if(_guard EQUAL -1 OR _features EQUAL -1)
    message(FATAL_ERROR
        "benchmarks/expected/CMakeLists.txt sets CMAKE_CXX_STANDARD 23 without asking "
        "CMAKE_CXX_COMPILE_FEATURES for cxx_std_23")
endif()
if(_guard GREATER _set)
    message(FATAL_ERROR
        "the cxx_std_23 test must come before set(CMAKE_CXX_STANDARD 23)")
endif()
# check_cxx_source_compiles does nothing when its result variable is already
# defined, so the variable is set to FALSE only in the branch without the check
# (set before it, it silently turned the std variant off on every compiler).
string(FIND "${_text}" "check_cxx_source_compiles(" _check)
string(FIND "${_text}" "set(LUMEX_BENCH_HAS_STD_EXPECTED FALSE)" _false)
if(_check EQUAL -1)
    message(FATAL_ERROR "benchmarks/expected/CMakeLists.txt lost its std::expected check")
endif()
if(NOT _false EQUAL -1 AND _false LESS _check)
    message(FATAL_ERROR
        "LUMEX_BENCH_HAS_STD_EXPECTED is set before check_cxx_source_compiles, which "
        "then skips the check")
endif()
# The result is cached as LUMEX_BENCH_HAS_STD_EXPECTED either way: the variant
# is skipped when it is false.
string(FIND "${_text}" "if(LUMEX_BENCH_HAS_STD_EXPECTED)" _use)
if(_use EQUAL -1)
    message(FATAL_ERROR "the std variant is no longer decided by LUMEX_BENCH_HAS_STD_EXPECTED")
endif()
