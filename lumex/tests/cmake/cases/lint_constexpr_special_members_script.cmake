# Scripts/CodeTools/check_constexpr_special_members.py (the
# lint.constexpr_special_members case): in fixtures/constexpr_special_members/
# bad/lumex it must report 18 hand-written constexpr special members: 16 in
# core/demo/Bad.hpp (a constructor, a destructor, one after and one before
# `explicit`, a defaulted one, LUMEX_CONSTEXPR on a constructor, a name on the
# line after `constexpr`, an attribute macro between them, LUMEX_CONSTEXPR_
# FUNCTION on a destructor, an out-of-line template constructor, a macro body,
# `explicit (condition)`, `__forceinline`, `consteval`, LUMEX_CONSTEXPR_CXX14 on a
# destructor, a constructor after `u8'a'` on its line)
# and 2 in core/pending/Pending.hpp. With --skip lumex/core/pending only the 16
# remain. Nothing in fixtures/constexpr_special_members/good/lumex (comments, a
# string, a raw string, a digit separator, the macros of the special members,
# `constexpr` on functions, variables, a conversion operator, a function
# pointer, a function name in parentheses, a macro return type, a lambda,
# `if constexpr`, u8'a'). A planted constexpr constructor and a
# planted destructor in a copy of the good tree are reported, then the copy is
# clean again. A checker that stops at the first hit, or reads a comment as
# code, lets the rule slip or fails on the macros that define it.

find_program(_python NAMES python3 python)
if(NOT _python)
    message(STATUS "Skipping lint_constexpr_special_members_script: no Python 3 interpreter")
    return()
endif()

set(_script "${LUMEX_SOURCE_DIR}/Scripts/CodeTools/check_constexpr_special_members.py")
set(_fixtures "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/fixtures/constexpr_special_members")

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/bad/lumex
    RESULT_VARIABLE _bad_code
    OUTPUT_VARIABLE _bad_out)
if(NOT _bad_code EQUAL 1)
    message(FATAL_ERROR "the checker returned ${_bad_code} for the bad fixture, expected 1:\n${_bad_out}")
endif()
if(NOT _bad_out MATCHES "18 hand-written constexpr special member\\(s\\)")
    message(FATAL_ERROR "the checker did not count 18 members in the bad fixture:\n${_bad_out}")
endif()
foreach(_where
        "Bad\\.hpp:7: constexpr on a constructor"
        "Bad\\.hpp:8: constexpr on a destructor"
        "Bad\\.hpp:9: constexpr on a constructor"
        "Bad\\.hpp:10: constexpr on a constructor"
        "Bad\\.hpp:11: constexpr on a constructor"
        "Bad\\.hpp:12: LUMEX_CONSTEXPR on a constructor"
        "Bad\\.hpp:13: constexpr on a constructor"
        "Bad\\.hpp:15: constexpr on a constructor"
        "Bad\\.hpp:16: LUMEX_CONSTEXPR_FUNCTION on a destructor"
        "Bad\\.hpp:23: constexpr on a constructor"
        "Bad\\.hpp:25: constexpr on a constructor"
        "Bad\\.hpp:29: constexpr on a constructor"
        "Bad\\.hpp:30: constexpr on a constructor"
        "Bad\\.hpp:31: consteval on a constructor"
        "Bad\\.hpp:32: LUMEX_CONSTEXPR_CXX14 on a destructor"
        "Bad\\.hpp:34: constexpr on a constructor"
        "Pending\\.hpp:4: constexpr on a constructor"
        "Pending\\.hpp:5: constexpr on a destructor")
    if(NOT _bad_out MATCHES "${_where}")
        message(FATAL_ERROR "the checker did not report ${_where}:\n${_bad_out}")
    endif()
endforeach()

# The list of modules that are not done yet.
execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/bad/lumex
        --skip lumex/core/pending
    RESULT_VARIABLE _skip_code
    OUTPUT_VARIABLE _skip_out)
if(NOT _skip_code EQUAL 1)
    message(FATAL_ERROR "the checker returned ${_skip_code} with a skipped directory, expected 1 (Bad.hpp stays):\n${_skip_out}")
endif()
if(NOT _skip_out MATCHES "16 hand-written constexpr special member\\(s\\)"
        OR _skip_out MATCHES "Pending\\.hpp")
    message(FATAL_ERROR "--skip did not remove exactly the pending directory:\n${_skip_out}")
endif()
execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/bad/lumex
        --skip lumex/core/pending --skip lumex/core/demo
    RESULT_VARIABLE _all_skipped_code
    OUTPUT_VARIABLE _all_skipped_out)
if(NOT _all_skipped_code EQUAL 0)
    message(FATAL_ERROR "the checker reported a skipped tree:\n${_all_skipped_out}")
endif()
# A skipped name is a directory, not the prefix of a name: core/dem skips nothing.
execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/bad/lumex
        --skip lumex/core/dem --skip lumex/core/pending
    RESULT_VARIABLE _prefix_code
    OUTPUT_VARIABLE _prefix_out)
if(NOT _prefix_code EQUAL 1)
    message(FATAL_ERROR "--skip matched the prefix of a directory name:\n${_prefix_out}")
endif()

execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_fixtures}/good/lumex
    RESULT_VARIABLE _good_code
    OUTPUT_VARIABLE _good_out)
if(NOT _good_code EQUAL 0)
    message(FATAL_ERROR "the checker rejected the good fixture:\n${_good_out}")
endif()

# A planted raw constexpr constructor, then a destructor, in a copy of the good
# tree; the macros of the special members in their place pass.
set(_work "${CMAKE_CURRENT_BINARY_DIR}/lint_constexpr_special_members_plant")
file(REMOVE_RECURSE "${_work}")
file(COPY "${_fixtures}/good/lumex" DESTINATION "${_work}")
set(_planted "${_work}/lumex/core/demo/Planted.hpp")
file(WRITE "${_planted}" "struct planted\n{\n  constexpr planted () {}\n};\n")
execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_work}/lumex
    RESULT_VARIABLE _ctor_code
    OUTPUT_VARIABLE _ctor_out)
if(NOT _ctor_code EQUAL 1 OR NOT _ctor_out MATCHES "Planted\\.hpp:3: constexpr on a constructor")
    message(FATAL_ERROR "a planted constexpr constructor was not reported (${_ctor_code}):\n${_ctor_out}")
endif()
file(WRITE "${_planted}" "struct planted\n{\n  constexpr ~planted () {}\n};\n")
execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_work}/lumex
    RESULT_VARIABLE _dtor_code
    OUTPUT_VARIABLE _dtor_out)
if(NOT _dtor_code EQUAL 1 OR NOT _dtor_out MATCHES "Planted\\.hpp:3: constexpr on a destructor")
    message(FATAL_ERROR "a planted constexpr destructor was not reported (${_dtor_code}):\n${_dtor_out}")
endif()
file(WRITE "${_planted}" "struct planted\n{\n  LUMEX_CONSTEXPR_CTOR planted () {}\n  LUMEX_CONSTEXPR_DTOR ~planted () {}\n};\n")
execute_process(
    COMMAND ${_python} ${_script} --check --dir ${_work}/lumex
    RESULT_VARIABLE _fixed_code
    OUTPUT_VARIABLE _fixed_out)
if(NOT _fixed_code EQUAL 0)
    message(FATAL_ERROR "the macros of the special members were reported:\n${_fixed_out}")
endif()
file(REMOVE_RECURSE "${_work}")
