# WindowsSEHTranslator.hpp has two branches, and only the Windows one can be
# compiled on Windows. This case reads both from the sources on any host.
#
# Windows: seh_translator is an ordinary exported function. The header
# declares it with LUMEX_PUBLIC_API and without `inline`, and the .cpp defines
# it the same way, so a consumer whose SET_SEH_TRANSLATOR takes its address
# from another translation unit links.
#
# Other platforms: SET_SEH_TRANSLATOR stays empty and the function stays
# undeclared (the declaration sits inside the Windows guard).

set(_header_path
    "${LUMEX_SOURCE_DIR}/lumex/core/exceptions/crash/WindowsSEHTranslator.hpp")
set(_source_path
    "${LUMEX_SOURCE_DIR}/lumex/core/exceptions/crash/WindowsSEHTranslator.cpp")
file(READ "${_header_path}" _header)
file(READ "${_source_path}" _source)

set(_space "[ \t\r\n]+")
set(_exported "LUMEX_PUBLIC_API${_space}void${_space}seh_translator[ \t]*\\(")
set(_inline "inline${_space}void${_space}seh_translator[ \t]*\\(")

set(_errors "")

foreach(_file IN ITEMS header source)
    string(REGEX MATCH "${_exported}" _hit "${_${_file}}")
    if(_hit STREQUAL "")
        list(APPEND _errors
            "the ${_file} does not give seh_translator LUMEX_PUBLIC_API void")
    endif()
    string(REGEX MATCH "${_inline}" _hit "${_${_file}}")
    if(NOT _hit STREQUAL "")
        list(APPEND _errors "the ${_file} declares seh_translator `inline`")
    endif()
endforeach()

# The declaration is the only text between the Windows guard and its #endif.
string(REGEX MATCH
    "#if defined\\(LUMEX_OS_WINDOWS\\)\n[^#]*seh_translator \\([^#]*\n#endif\n"
    _guarded "${_header}")
if(_guarded STREQUAL "")
    list(APPEND _errors
        "the declaration of seh_translator is not inside "
        "#if defined(LUMEX_OS_WINDOWS) ... #endif")
endif()

# string(CONCAT) keeps the `;` that a list built by set() would split on.
string(CONCAT _macro_branches
    "#if defined(LUMEX_OS_WINDOWS)\n"
    "#define SET_SEH_TRANSLATOR _set_se_translator (seh_translator);\n"
    "#else\n"
    "#define SET_SEH_TRANSLATOR\n"
    "#endif\n")
set(_expected_block "")
string(FIND "${_header}" "${_macro_branches}" _at)
if(_at EQUAL -1)
    list(APPEND _errors
        "SET_SEH_TRANSLATOR is no longer the Windows call with an empty "
        "macro elsewhere")
    set(_expected_block "\nexpected:\n${_macro_branches}")
endif()

if(_errors)
    string(REPLACE ";" "\n  " _list "${_errors}")
    message(FATAL_ERROR "WindowsSEHTranslator:\n  ${_list}${_expected_block}")
endif()
