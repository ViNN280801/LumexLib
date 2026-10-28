# Post-build check of the fixture's shared library (ELF): its dynamic symbol
# table holds the fixture's own functions and nothing of the static transport
# or hidapi that is linked into it (their globals stay one per binary). The
# symbols of Asio are the exception, on purpose: its headers export them, and
# an io_context shared by two binaries needs one set of them per process.
# Run with -DNM=<nm> -DLIBRARY=<file>.

execute_process(
    COMMAND "${NM}" --dynamic --defined-only --demangle "${LIBRARY}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _symbols
    ERROR_VARIABLE _errors)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "nm failed on ${LIBRARY}: ${_errors}")
endif()

string(REPLACE "\n" ";" _lines "${_symbols}")
set(_leaked "")
set(_own 0)
foreach(_line IN LISTS _lines)
    # "<address> <type> <name>": the names that matter start after the type.
    if(NOT _line MATCHES "^[0-9a-fA-F]+ [A-Za-z] (.+)$")
        continue()
    endif()
    set(_name "${CMAKE_MATCH_1}")
    if(_name MATCHES "^fixture::")
        math(EXPR _own "${_own} + 1")
    else()
        # An Asio symbol is allowed even when its template arguments name a
        # type of the transport (an instantiation over its message type is
        # exported with the rest of Asio): judge the name up to its first
        # template or parameter list.
        string(REGEX REPLACE "[<(].*$" "" _head "${_name}")
        if(NOT _head MATCHES "asio::" AND _name MATCHES "hid_|lumex|transport")
            list(APPEND _leaked "${_name}")
        endif()
    endif()
endforeach()

if(_own LESS 3)
    message(FATAL_ERROR
        "${LIBRARY} exports ${_own} fixture function(s), expected 3: the check "
        "does not see its own symbols")
endif()
if(_leaked)
    list(LENGTH _leaked _count)
    list(SUBLIST _leaked 0 10 _first)
    string(REPLACE ";" "\n  " _first "${_first}")
    message(FATAL_ERROR
        "${LIBRARY} exports ${_count} symbol(s) of the static transport or "
        "hidapi it links (they must be built with hidden visibility), "
        "e.g.:\n"
        "  ${_first}")
endif()
message(STATUS "${LIBRARY}: ${_own} fixture symbols exported, none of the "
        "transport or hidapi")
