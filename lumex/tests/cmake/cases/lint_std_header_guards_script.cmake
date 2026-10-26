# Scripts/CodeTools/check_std_header_guards.py (the lint.std_header_guards
# case): it must report the five includes of fixtures/std_header_guards/bad
# (__has_include only, no guard, a standard too old, the #else branch, a
# condition with ||) and nothing in fixtures/std_header_guards/good. A checker
# that accepts the bad file lets an include through that the MSVC STL reports
# with STL4038 at an older standard.

find_program(_python NAMES python3 python)
if(NOT _python)
    message(STATUS "Skipping lint_std_header_guards_script: no Python 3 interpreter")
    return()
endif()

set(_script "${LUMEX_SOURCE_DIR}/Scripts/CodeTools/check_std_header_guards.py")
set(_fixtures "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/fixtures/std_header_guards")

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/bad/lumex
    RESULT_VARIABLE _bad_code
    OUTPUT_VARIABLE _bad_out)
if(_bad_code EQUAL 0)
    message(FATAL_ERROR "the checker accepted the bad fixture:\n${_bad_out}")
endif()
if(NOT _bad_out MATCHES "5 unguarded standard header include")
    message(FATAL_ERROR "the checker did not find 5 includes in the bad fixture:\n${_bad_out}")
endif()
foreach(_header compare optional span expected bit)
    if(NOT _bad_out MATCHES "<${_header}>")
        message(FATAL_ERROR "the checker did not report <${_header}>:\n${_bad_out}")
    endif()
endforeach()

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/good/lumex
    RESULT_VARIABLE _good_code
    OUTPUT_VARIABLE _good_out)
if(NOT _good_code EQUAL 0)
    message(FATAL_ERROR "the checker rejected the good fixture:\n${_good_out}")
endif()
