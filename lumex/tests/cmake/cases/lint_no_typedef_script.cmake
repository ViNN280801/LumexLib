# Scripts/CodeTools/check_no_typedef.py (the lint.no_typedef case): in
# fixtures/no_typedef/bad/lumex it must report 14 typedefs: two in the
# continuation lines of a macro, one in a Windows-only branch, one in a branch
# that is never taken and whose keyword ends its line, a function pointer, a
# class without a name, a list of declarators, one in a header without an
# extension, one in a fenced block of a Markdown file, one with a trailing
# const, one with a trailing attribute, one after a digit separator on its
# line, one in a .inl file and one under a directory named tests; nothing in
# fixtures/no_typedef/good/lumex (typedef in comments, strings, a raw string,
# identifiers that contain the word, prose and a CMake block of a Markdown
# file). A file named with --c-exception is skipped. A trailing name in
# capitals (an attribute macro?) is reported and --fix leaves it. --fix
# converts the bad fixture (a copy) and leaves nothing behind. A checker that misses the macro
# or the Windows branch lets a typedef in that the Linux compiler never sees.

find_program(_python NAMES python3 python)
if(NOT _python)
    message(STATUS "Skipping lint_no_typedef_script: no Python 3 interpreter")
    return()
endif()

set(_script "${LUMEX_SOURCE_DIR}/Scripts/CodeTools/check_no_typedef.py")
set(_fixtures "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/fixtures/no_typedef")

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/bad/lumex
    RESULT_VARIABLE _bad_code
    OUTPUT_VARIABLE _bad_out)
if(NOT _bad_code EQUAL 1)
    message(FATAL_ERROR "the checker returned ${_bad_code} for the bad fixture, expected 1:\n${_bad_out}")
endif()
if(NOT _bad_out MATCHES "14 typedef\\(s\\)")
    message(FATAL_ERROR "the checker did not count 14 typedefs in the bad fixture:\n${_bad_out}")
endif()
foreach(_where
        "lumex/Macros\\.hpp:4" "lumex/Macros\\.hpp:5"
        "lumex/Windows\\.hpp:4" "lumex/Windows\\.hpp:9"
        "lumex/Source\\.cpp:2" "lumex/Source\\.cpp:4" "lumex/Source\\.cpp:10"
        "lumex/Umbrella:2" "lumex/README\\.md:6"
        "lumex/Source\\.cpp:12" "lumex/Source\\.cpp:14" "lumex/Source\\.cpp:16"
        "lumex/Extra\\.inl:2" "lumex/tests/Check\\.cpp:2")
    if(NOT _bad_out MATCHES "${_where}: typedef")
        message(FATAL_ERROR "the checker did not report ${_where}:\n${_bad_out}")
    endif()
endforeach()

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/good/lumex
    RESULT_VARIABLE _good_code
    OUTPUT_VARIABLE _good_out)
if(NOT _good_code EQUAL 0)
    message(FATAL_ERROR "the checker rejected the good fixture:\n${_good_out}")
endif()

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/exception/lumex
    RESULT_VARIABLE _plain_code
    OUTPUT_VARIABLE _plain_out)
if(NOT _plain_code EQUAL 1)
    message(FATAL_ERROR "the checker accepted a typedef without an exception:\n${_plain_out}")
endif()
execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/exception/lumex
        --c-exception lumex/CApi.h
    RESULT_VARIABLE _listed_code
    OUTPUT_VARIABLE _listed_out)
if(NOT _listed_code EQUAL 0)
    message(FATAL_ERROR "the checker did not skip the file on the exception list:\n${_listed_out}")
endif()

# --fix on a copy: nothing is left, and the declarations read as aliases.
set(_work "${CMAKE_CURRENT_BINARY_DIR}/lint_no_typedef_fix")
file(REMOVE_RECURSE "${_work}")
file(COPY "${_fixtures}/bad/lumex" DESTINATION "${_work}")
execute_process(
    COMMAND ${_python} ${_script} --fix --dir ${_work}/lumex
    RESULT_VARIABLE _fix_code
    OUTPUT_VARIABLE _fix_out
    ERROR_VARIABLE _fix_err)
if(NOT _fix_code EQUAL 0 OR NOT _fix_out MATCHES "14 typedef\\(s\\) converted, 0 left")
    message(FATAL_ERROR "--fix returned ${_fix_code}:\n${_fix_out}${_fix_err}")
endif()
execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_work}/lumex
    RESULT_VARIABLE _after_code
    OUTPUT_VARIABLE _after_out)
if(NOT _after_code EQUAL 0)
    message(FATAL_ERROR "typedefs are left after --fix:\n${_after_out}")
endif()
file(READ "${_work}/lumex/Macros.hpp" _macros)
file(READ "${_work}/lumex/Source.cpp" _source)
file(READ "${_work}/lumex/Windows.hpp" _windows)
foreach(_expect
        "using name##_type = int;"
        "using name##_list = std::vector<name##_type>;")
    string(FIND "${_macros}" "${_expect}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "--fix did not write '${_expect}':\n${_macros}")
    endif()
endforeach()
foreach(_expect
        "using callback_t = void (*) (int code, char const *text);"
        "struct pair_t"
        "using value_t = int;"
        "using pointer_t = int *;"
        "using const_alias_t = A const;"
        "using wide_t __attribute__((may_alias)) = unsigned __int128;"
        "using after_digits_t = long;")
    string(FIND "${_source}" "${_expect}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "--fix did not write '${_expect}':\n${_source}")
    endif()
endforeach()
if(NOT _windows MATCHES "using dword_t = unsigned long;"
        OR NOT _windows MATCHES "using split_t = unsigned long long;")
    message(FATAL_ERROR "--fix did not convert the Windows.hpp declarations:\n${_windows}")
endif()
file(REMOVE_RECURSE "${_work}")

# A trailing name in capitals may be an attribute macro: reported by --check,
# refused (with a message) and left untouched by --fix.
set(_refuse "${CMAKE_CURRENT_BINARY_DIR}/lint_no_typedef_refuse")
file(REMOVE_RECURSE "${_refuse}")
file(COPY "${_fixtures}/refuse/lumex" DESTINATION "${_refuse}")
execute_process(
    COMMAND ${_python} ${_script} --fix --dir ${_refuse}/lumex
    RESULT_VARIABLE _refuse_code
    OUTPUT_VARIABLE _refuse_out
    ERROR_VARIABLE _refuse_err)
file(READ "${_refuse}/lumex/Macro.hpp" _refused)
if(NOT _refuse_code EQUAL 1 OR NOT _refuse_err MATCHES "attribute macro"
        OR NOT _refuse_out MATCHES "0 typedef\\(s\\) converted, 1 left"
        OR NOT _refused MATCHES "typedef data<S> d BENCH_UNUSED;")
    message(FATAL_ERROR "--fix did not refuse the attribute macro (${_refuse_code}):\n${_refuse_out}${_refuse_err}\n${_refused}")
endif()
file(REMOVE_RECURSE "${_refuse}")
