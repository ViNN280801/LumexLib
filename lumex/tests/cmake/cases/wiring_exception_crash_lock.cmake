# The exception suites share bin/ and TearDown removes <exe dir>/crashes.
# to_crash_report has no other directory. The lock must stay in the
# CMakeLists that registers those suites, not only in a comment.

file(READ
    "${LUMEX_SOURCE_DIR}/lumex/tests/core/exceptions/CMakeLists.txt"
    _exceptions)
string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" _code "${_exceptions}")
string(FIND "${_code}" "RESOURCE_LOCK lumex_exception_crash_dir" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "lumex/tests/core/exceptions/CMakeLists.txt: "
        "RESOURCE_LOCK lumex_exception_crash_dir is missing from the "
        "code. The three exception executables share bin/crashes.")
endif()
