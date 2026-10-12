# MSVC link of a test exe fails with LNK1104 (cannot open the output exe)
# when gtest discovery runs that exe from the link recipe (POST_BUILD) and
# /INCREMENTAL tries to patch the same file. Discovery must be PRE_TEST, and
# test binaries must link with /INCREMENTAL:NO.

set(_gtest "${LUMEX_SOURCE_DIR}/cmake/LumexGoogleTest.cmake")
file(READ "${_gtest}" _helper)
if(NOT _helper MATCHES "function\\(lumex_gtest_discover_tests")
    message(FATAL_ERROR "lumex_gtest_discover_tests is missing from LumexGoogleTest.cmake")
endif()
if(NOT _helper MATCHES "DISCOVERY_MODE PRE_TEST")
    message(FATAL_ERROR "lumex_gtest_discover_tests must pass DISCOVERY_MODE PRE_TEST")
endif()
if(NOT _helper MATCHES "/INCREMENTAL:NO")
    message(FATAL_ERROR "lumex_test_use_gtest must pass /INCREMENTAL:NO on MSVC")
endif()

file(GLOB_RECURSE _lists
    "${LUMEX_SOURCE_DIR}/lumex/tests/CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/tests/*/CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/tests/*/*/CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/tests/*/*/*/CMakeLists.txt"
)
if(NOT _lists)
    message(FATAL_ERROR "no test CMakeLists.txt under lumex/tests")
endif()

set(_raw_calls "")
foreach(_list IN LISTS _lists)
    file(STRINGS "${_list}" _lines)
    set(_n 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _n "${_n} + 1")
        if(_line MATCHES "(^|[^_])gtest_discover_tests\\(" AND NOT _line MATCHES "^[ \t]*#")
            string(APPEND _raw_calls "  ${_list}:${_n}: ${_line}\n")
        endif()
    endforeach()
endforeach()
if(_raw_calls)
    message(FATAL_ERROR
        "test CMakeLists must call lumex_gtest_discover_tests, not gtest_discover_tests:\n${_raw_calls}")
endif()
