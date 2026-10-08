# Export of the string_view library on Windows. LUMEX_API is dllexport in every
# DLL built with LUMEX_EXPORTS, so a module that includes the view headers
# would export the view operators again (MinGW: six extra symbols in each of
# base64, crc, exceptions and xml). The views use LUMEX_STRING_VIEW_API instead,
# keyed on LumexCore_string_view_EXPORTS, like LUMEX_UTILITY_API; the six
# comparison operators are plain inline functions.
#
# Part 1 reads the sources. Part 2 compiles one translation unit of each of
# those modules with a MinGW compiler (when one is installed) the way the DLL
# is built and reads the export directives (.drectve) of the object file: there
# must be none for string_view, and the string_view library itself must export.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_forbid_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} mentions ${needle}")
    endif()
endfunction()

# --- part 1: the sources ---------------------------------------------------
_require_text("lumex/LumexExport.hpp"
    "#if defined(LumexCore_string_view_EXPORTS)\n#define LUMEX_STRING_VIEW_API __declspec (dllexport)\n#else\n#define LUMEX_STRING_VIEW_API __declspec (dllimport)")
_require_text("lumex/LumexExport.hpp"
    "#elif defined(LumexCore_string_view_EXPORTS)\n#define LUMEX_STRING_VIEW_API __attribute__ ((visibility (\"default\")))")
foreach(_header LumexStringView.hpp LumexWStringView.hpp)
    set(_path "lumex/core/string_view/view/${_header}")
    string(REGEX REPLACE "\\.hpp$" "" _stem "${_header}")
    string(REGEX REPLACE "^Lumex" "" _stem "${_stem}")
    _forbid_text("${_path}" "LUMEX_API")
    _require_text("${_path}" "class LUMEX_STRING_VIEW_API lumex_")
    _require_text("${_path}" "LUMEX_STRING_VIEW_API std::")
    file(READ "${LUMEX_SOURCE_DIR}/${_path}" _txt)
    # Six comparison operators, all plain inline functions.
    string(REGEX MATCHALL "\ninline bool\noperator(==|!=|<|>|<=|>=) \\(lumex_w?string_view lhs, lumex_w?string_view rhs\\)" _ops "${_txt}")
    list(LENGTH _ops _count)
    if(NOT _count EQUAL 6)
        message(FATAL_ERROR "${_path}: ${_count} plain inline comparison operators, expected 6")
    endif()
endforeach()

# --- part 2: the object files of a MinGW build -----------------------------
find_program(_mingw NAMES x86_64-w64-mingw32-g++-posix
                          x86_64-w64-mingw32-g++-win32
                          x86_64-w64-mingw32-g++)
find_program(_strings NAMES strings)
if(NOT _mingw OR NOT _strings)
    message(STATUS "no MinGW compiler or strings: only the sources were checked")
    return()
endif()

set(_work "${CMAKE_BINARY_DIR}/string_view_export")
file(MAKE_DIRECTORY "${_work}")

file(GLOB_RECURSE _xml_entries LIST_DIRECTORIES true "${LUMEX_SOURCE_DIR}/lumex/xml/*")
set(_xml_includes "")
foreach(_entry IN LISTS _xml_entries)
    if(IS_DIRECTORY "${_entry}")
        list(APPEND _xml_includes "-I${_entry}")
    endif()
endforeach()

# The export directives of one translation unit, compiled as part of a DLL.
function(_exports out_var source)
    get_filename_component(_name "${source}" NAME_WE)
    set(_object "${_work}/${_name}.o")
    if(IS_ABSOLUTE "${source}")
        set(_source_path "${source}")
    else()
        set(_source_path "${LUMEX_SOURCE_DIR}/${source}")
    endif()
    execute_process(
        COMMAND "${_mingw}" -std=c++11 -c -DLUMEX_EXPORTS -DNOMINMAX
            -DWIN32_LEAN_AND_MEAN "-I${LUMEX_SOURCE_DIR}" ${_xml_includes}
            ${ARGN} "${_source_path}" -o "${_object}"
        RESULT_VARIABLE _rv
        ERROR_VARIABLE _err)
    if(NOT _rv EQUAL 0)
        message(FATAL_ERROR "${source} does not compile with ${_mingw}:\n${_err}")
    endif()
    execute_process(COMMAND "${_strings}" -a "${_object}"
        OUTPUT_VARIABLE _text)
    string(REGEX MATCHALL "-export:\"[^\"]*\"" _all "${_text}")
    set(${out_var} "${_all}" PARENT_SCOPE)
endfunction()

# The macro itself, preprocessed by the Windows compiler: imported in a DLL
# built with LUMEX_EXPORTS that does not own the views (GCC exports nothing for
# a dllexport class whose inline members are unused, so the object files below
# cannot tell), exported only from the string_view library.
set(_macro_source "${_work}/Macro.cpp")
file(WRITE "${_macro_source}" [=[
#include "lumex/LumexExport.hpp"
view_api: LUMEX_STRING_VIEW_API
generic_api: LUMEX_API
]=])
function(_expansion out_var)
    execute_process(
        COMMAND "${_mingw}" -E -P -x c++ "-I${LUMEX_SOURCE_DIR}" ${ARGN}
            "${_macro_source}"
        OUTPUT_VARIABLE _text RESULT_VARIABLE _rv)
    if(NOT _rv EQUAL 0)
        message(FATAL_ERROR "${_mingw} cannot preprocess ${_macro_source}")
    endif()
    set(${out_var} "${_text}" PARENT_SCOPE)
endfunction()
_expansion(_in_module_dll -DLUMEX_EXPORTS)
if(NOT _in_module_dll MATCHES "view_api:[^\n]*dllimport"
   OR NOT _in_module_dll MATCHES "generic_api:[^\n]*dllexport")
    message(FATAL_ERROR
        "in a DLL built with LUMEX_EXPORTS the views must be imported while "
        "LUMEX_API is exported:\n${_in_module_dll}")
endif()
_expansion(_in_view_dll -DLUMEX_EXPORTS -DLumexCore_string_view_EXPORTS)
if(NOT _in_view_dll MATCHES "view_api:[^\n]*dllexport")
    message(FATAL_ERROR
        "the string_view library must export its views:\n${_in_view_dll}")
endif()
_expansion(_in_consumer)
if(NOT _in_consumer MATCHES "view_api:[^\n]*dllimport")
    message(FATAL_ERROR
        "a consumer must import the views:\n${_in_consumer}")
endif()

foreach(_source
        lumex/core/base64/encode/Encoder.cpp
        lumex/core/crc/catalog/LumexCrcCatalog.cpp
        lumex/core/exceptions/exception/LumexException.cpp
        lumex/xml/attribute/XmlAttribute.cpp)
    _exports(_exports_of "${_source}")
    set(_view_exports "")
    foreach(_directive IN LISTS _exports_of)
        if(_directive MATCHES "string_view")
            list(APPEND _view_exports "${_directive}")
        endif()
    endforeach()
    if(_view_exports)
        message(FATAL_ERROR
            "${_source} exports string_view symbols from its DLL:\n${_view_exports}")
    endif()
    # The modules export their own functions: a result without any directive
    # means the check reads nothing.
    list(LENGTH _exports_of _own)
    if(_own EQUAL 0)
        message(FATAL_ERROR "${_source}: no export directive at all, the check reads nothing")
    endif()
endforeach()

# The control: the string_view library exports its own class.
_exports(_view_library lumex/core/string_view/view/LumexStringView.cpp
         -DLumexCore_string_view_EXPORTS)
set(_own_view 0)
foreach(_directive IN LISTS _view_library)
    if(_directive MATCHES "lumex_string_view")
        math(EXPR _own_view "${_own_view} + 1")
    endif()
endforeach()
if(_own_view EQUAL 0)
    message(FATAL_ERROR "the string_view library exports nothing of lumex_string_view")
endif()
