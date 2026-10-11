# Export of the string_view views on Windows. The module is header-only: its
# classes are templates and every member is inline, so there is no library to
# export from and no export macro (LUMEX_STRING_VIEW_API is gone, and with it
# the six comparison operators that needed it). What a DLL of another module
# must still not do is export the views: LUMEX_API is dllexport in every DLL
# built with LUMEX_EXPORTS, and a header that used it would make base64, crc,
# exceptions and xml export the view members again.
#
# Part 1 reads the sources: no export macro in the view headers or in
# LumexExport.hpp. Part 2 compiles one translation unit of each of those
# modules with a MinGW compiler (when one is installed) the way the DLL is
# built and reads the export directives (.drectve) of the object file: there
# must be none for string_view.

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
_forbid_text("lumex/LumexExport.hpp" "LUMEX_STRING_VIEW_API")
_forbid_text("lumex/LumexExport.hpp" "LumexCore_string_view_EXPORTS")
foreach(_header LumexBasicStringView.hpp LumexStringView.hpp LumexWStringView.hpp)
    set(_path "lumex/core/string_view/view/${_header}")
    _forbid_text("${_path}" "LUMEX_API")
    _forbid_text("${_path}" "LUMEX_STRING_VIEW_API")
    _forbid_text("${_path}" "LUMEX_PUBLIC_API")
    _forbid_text("${_path}" "LumexExport.hpp")
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
