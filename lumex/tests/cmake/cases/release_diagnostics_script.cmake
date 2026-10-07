# Scripts/ReleaseTools/extract_diagnostics.py, which create_release.sh runs
# after every build to fill <output-dir>/warns/. Each fixture is a piece of a
# real GCC, Clang or MinGW build log. The case checks the counts the script
# prints and which of the two files it writes (a file exists only when it has
# something in it). A wrong class or a lost diagnostic changes a count.

find_program(_python NAMES python3 python)
if(NOT _python)
    message(STATUS "Skipping release_diagnostics_script: no Python 3 interpreter")
    return()
endif()

set(_script "${LUMEX_SOURCE_DIR}/Scripts/ReleaseTools/extract_diagnostics.py")
set(_fixtures "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/fixtures/release_diagnostics")
set(_work "${CMAKE_CURRENT_BINARY_DIR}/release_diagnostics_work")

# _check(<fixture> <failed 0|1> <warnings> <errors> <warn file 0|1> <err file 0|1>)
function(_check fixture failed warnings errors warn_file err_file)
    file(REMOVE_RECURSE "${_work}")
    file(MAKE_DIRECTORY "${_work}")
    set(_args "")
    if(failed)
        set(_args --failed)
    endif()
    execute_process(
        COMMAND ${_python} ${_script} ${_fixtures}/${fixture}.log
            --warn-out ${_work}/warn.log --err-out ${_work}/err.log ${_args}
        RESULT_VARIABLE _code
        OUTPUT_VARIABLE _out
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _code EQUAL 0)
        message(FATAL_ERROR "${fixture}: the extractor exited with ${_code}")
    endif()
    if(NOT _out STREQUAL "warnings=${warnings} errors=${errors}")
        message(FATAL_ERROR
            "${fixture}: expected 'warnings=${warnings} errors=${errors}', got '${_out}'")
    endif()
    foreach(_kind warn err)
        set(_want ${${_kind}_file})
        if(_want AND NOT EXISTS "${_work}/${_kind}.log")
            message(FATAL_ERROR "${fixture}: ${_kind}.log was not written")
        endif()
        if(NOT _want AND EXISTS "${_work}/${_kind}.log")
            message(FATAL_ERROR "${fixture}: ${_kind}.log was written, it should not exist")
        endif()
    endforeach()
endfunction()

# The same warning from a header that two sources include counts once.
_check(gcc_warnings 0 2 0 1 0)
# A build -Werror stopped is about its warnings: errors stay 0.
_check(gcc_werror 1 1 0 1 0)
_check(clang_werror 1 1 0 1 0)
# A compile error, an undefined reference, the linker's own line and a
# <built-in> warning that has no line number.
_check(real_errors 1 1 3 1 1)
# A failure without any diagnostic keeps the end of the log.
_check(plain_failure 1 0 1 0 1)
_check(clean 0 0 0 0 0)

file(REMOVE_RECURSE "${_work}")
