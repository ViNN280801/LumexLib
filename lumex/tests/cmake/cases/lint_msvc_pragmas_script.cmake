# Scripts/CodeTools/check_msvc_pragmas.py (the lint.msvc_pragmas case): it must
# report the three pragmas of fixtures/msvc_pragmas/bad (guarded by _WIN32, in
# the #else branch of _MSC_VER, in a condition with ||) and nothing in
# fixtures/msvc_pragmas/good. A checker that accepts the bad file lets a
# pragma through that GCC and MinGW report as unknown.

find_program(_python NAMES python3 python)
if(NOT _python)
    message(STATUS "Skipping lint_msvc_pragmas_script: no Python 3 interpreter")
    return()
endif()

set(_script "${LUMEX_SOURCE_DIR}/Scripts/CodeTools/check_msvc_pragmas.py")
set(_fixtures "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/fixtures/msvc_pragmas")

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/bad/lumex
    RESULT_VARIABLE _bad_code
    OUTPUT_VARIABLE _bad_out)
if(_bad_code EQUAL 0)
    message(FATAL_ERROR "the checker accepted the bad fixture:\n${_bad_out}")
endif()
if(NOT _bad_out MATCHES "3 unguarded MSVC pragma")
    message(FATAL_ERROR "the checker did not find 3 pragmas in the bad fixture:\n${_bad_out}")
endif()

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/good/lumex
    RESULT_VARIABLE _good_code
    OUTPUT_VARIABLE _good_out)
if(NOT _good_code EQUAL 0)
    message(FATAL_ERROR "the checker rejected the good fixture:\n${_good_out}")
endif()
