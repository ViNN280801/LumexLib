# Run by cmake.single_eh_model (an MSVC-style tree that writes
# compile_commands.json).
#
# Every compile command of this tree names one exception-handling model. Two
# of them (/EHa from the directory flags, then /EHsc from the HIGH target
# options) make cl.exe print D9025, which /WX does not stop, and the last one
# wins: the exceptions suites that need /EHa for _set_se_translator were
# built with /EHsc.
#
# Required: COMPILE_COMMANDS (the compile_commands.json of the tree).

if(NOT DEFINED COMPILE_COMMANDS OR NOT EXISTS "${COMPILE_COMMANDS}")
    message(FATAL_ERROR "single_eh_model: COMPILE_COMMANDS is required: ${COMPILE_COMMANDS}")
endif()

file(STRINGS "${COMPILE_COMMANDS}" _commands REGEX "\"command\"")
list(LENGTH _commands _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "single_eh_model: no command in ${COMPILE_COMMANDS}")
endif()

set(_bad "")
foreach(_command IN LISTS _commands)
    string(REGEX MATCHALL "[ \"][/-]EH[a-z]+" _models "${_command}")
    list(TRANSFORM _models REPLACE "^[ \"][/-]" "")
    list(REMOVE_DUPLICATES _models)
    list(LENGTH _models _distinct)
    if(_distinct GREATER 1)
        string(REGEX MATCH "[/-]Fo[^ ]+" _object "${_command}")
        list(JOIN _models ", " _models_text)
        list(APPEND _bad "${_object}: ${_models_text}")
    endif()
endforeach()

if(_bad)
    list(LENGTH _bad _bad_count)
    list(JOIN _bad "\n  " _bad_text)
    message(FATAL_ERROR
        "single_eh_model: ${_bad_count} of ${_count} commands name two EH models:\n  ${_bad_text}")
endif()
message(STATUS "single_eh_model: ${_count} commands, one EH model each")
