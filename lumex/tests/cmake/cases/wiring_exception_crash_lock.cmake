# The exception suites share bin/ and TearDown removes <exe dir>/crashes.
# to_crash_report has no other directory. The lock must stay in the
# CMakeLists that registers those suites, not only in a comment, in the
# directory of the module and in every directory below it that builds suites.

foreach(_directory "" "crash/" "exception/" "stacktrace/")
    set(_path "lumex/tests/core/exceptions/${_directory}CMakeLists.txt")
    file(READ "${LUMEX_SOURCE_DIR}/${_path}" _exceptions)
    string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" _code "${_exceptions}")
    string(FIND "${_code}" "RESOURCE_LOCK lumex_exception_crash_dir" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR
            "${_path}: "
            "RESOURCE_LOCK lumex_exception_crash_dir is missing from the "
            "code. The exception executables of every directory and "
            "standard share bin/crashes.")
    endif()
endforeach()
