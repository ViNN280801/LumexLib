# GoogleTest 1.12.1 sets no_sanitize_address for Clang only, so under GCC's
# AddressSanitizer StackGrowsDown () of its death-test code keeps its local on
# the use-after-return fake stack and every threadsafe death test kills the
# runner (SEGV in __clone). The vendored 1.12.1 library alone is compiled
# without that instrumentation; 1.18.0 needs nothing.

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexGoogleTest.cmake" _gtest)

string(FIND "${_gtest}" "--param=asan-use-after-return=0" _flag_pos)
if(_flag_pos EQUAL -1)
    message(FATAL_ERROR
        "cmake/LumexGoogleTest.cmake does not pass "
        "--param=asan-use-after-return=0 to the GoogleTest 1.12.1 library")
endif()

if(NOT _gtest MATCHES
   "if\\(LUMEX_USE_ASAN AND CMAKE_CXX_COMPILER_ID STREQUAL \"GNU\"\\)[ \t\r\n]+target_compile_options\\(lumex_gtest_1_12 PRIVATE --param=asan-use-after-return=0\\)")
    message(FATAL_ERROR
        "the use-after-return flag must be guarded by LUMEX_USE_ASAN and the "
        "GNU compiler id and applied to lumex_gtest_1_12 only")
endif()

string(REGEX MATCHALL "--param=asan-use-after-return=0" _flags "${_gtest}")
list(LENGTH _flags _flag_count)
if(NOT _flag_count EQUAL 1)
    message(FATAL_ERROR
        "the use-after-return flag must appear exactly once (found ${_flag_count})")
endif()
