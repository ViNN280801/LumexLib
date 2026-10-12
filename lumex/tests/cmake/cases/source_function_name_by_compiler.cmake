# LUMEX_FUNCTION_NAME is chosen by compiler, not by target. MinGW is GCC (or
# Clang) with _WIN32 defined, and GCC has no __FUNCSIG__: a choice keyed on
# _WIN32 broke every MinGW build that used the macro. This case pins the
# selection in LumexMacros.hpp and keeps it the only definition, so a second
# copy (LumexLoggingMacro.hpp had one) cannot drift from it.

set(_macros "lumex/core/utility/macros/LumexMacros.hpp")
file(READ "${LUMEX_SOURCE_DIR}/${_macros}" _text)

string(CONCAT _expected
    "#if defined(_MSC_VER)\n"
    "#define LUMEX_FUNCTION_NAME __FUNCSIG__\n"
    "#elif defined(__GNUC__) || defined(__clang__)\n"
    "#define LUMEX_FUNCTION_NAME __PRETTY_FUNCTION__\n"
    "#else\n"
    "#define LUMEX_FUNCTION_NAME __func__\n"
    "#endif\n")

# A plain string, not a list: the messages are joined as they are.
set(_errors "")
string(FIND "${_text}" "${_expected}" _at)
if(_at EQUAL -1)
    string(APPEND _errors
        "${_macros} does not choose LUMEX_FUNCTION_NAME by compiler, "
        "expected:\n${_expected}")
endif()

file(GLOB_RECURSE _headers RELATIVE "${LUMEX_SOURCE_DIR}"
    "${LUMEX_SOURCE_DIR}/lumex/*.hpp")
list(FILTER _headers EXCLUDE REGEX "^lumex/(tests|examples)/")
list(REMOVE_ITEM _headers "${_macros}")

foreach(_header IN LISTS _headers)
    file(READ "${LUMEX_SOURCE_DIR}/${_header}" _other)
    string(REGEX MATCH "#[ \t]*define[ \t]+LUMEX_FUNCTION_NAME[ \t(]" _hit
        "${_other}")
    if(NOT _hit STREQUAL "")
        string(APPEND _errors
            "${_header} defines LUMEX_FUNCTION_NAME again, include "
            "${_macros} instead\n")
    endif()
endforeach()

if(NOT _errors STREQUAL "")
    message(FATAL_ERROR "LUMEX_FUNCTION_NAME:\n${_errors}")
endif()
