# The XML wide-character mode is gone (user decision of 2026-10-08, MAJOR
# 2.0.0.0): no CMake option, no compile definition, no install or conan switch,
# no preprocessor branch in lumex/xml, and none of the macros that existed only
# for it (LUMEX_XML_CHAR, LUMEX_XML_TEXT, LUMEX_XML_MSVC_CRT_VERSION). The
# character type of the module is `char` (`char_t` stays as a plain alias).

# The names are built from parts so that this file does not contain them.
set(_prefix "LUMEX_XML")
set(_gone
    "${_prefix}_WCHAR_MODE"
    "${_prefix}_CHAR"
    "${_prefix}_TEXT("
    "${_prefix}_TEXT (")

set(_files
    "${LUMEX_SOURCE_DIR}/CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/conanfile.py"
    "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake"
    "${LUMEX_SOURCE_DIR}/cmake/LumexLibConfig.cmake.in"
    "${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake"
    "${LUMEX_SOURCE_DIR}/lumex/CMakeLists.txt")
file(GLOB_RECURSE _xml_sources
    "${LUMEX_SOURCE_DIR}/lumex/xml/*.hpp"
    "${LUMEX_SOURCE_DIR}/lumex/xml/*.cpp"
    "${LUMEX_SOURCE_DIR}/lumex/xml/CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/xml/LumexXml")
file(GLOB_RECURSE _xml_users
    "${LUMEX_SOURCE_DIR}/lumex/examples/xml/*.cpp"
    "${LUMEX_SOURCE_DIR}/lumex/tests/xml/*.cpp"
    "${LUMEX_SOURCE_DIR}/lumex/tests/xml/*.hpp")
list(APPEND _files ${_xml_sources} ${_xml_users})

foreach(_file IN LISTS _files)
    if(NOT EXISTS "${_file}")
        continue()
    endif()
    file(READ "${_file}" _text)
    foreach(_name IN LISTS _gone)
        string(FIND "${_text}" "${_name}" _pos)
        if(NOT _pos EQUAL -1)
            message(FATAL_ERROR
                "${_file} mentions '${_name}': the XML wide-character mode "
                "is gone and must not come back")
        endif()
    endforeach()
    string(FIND "${_text}" "${_prefix}_MSVC_CRT_VERSION" _crt)
    if(NOT _crt EQUAL -1)
        message(FATAL_ERROR
            "${_file}: ${_prefix}_MSVC_CRT_VERSION is gone (MSVC older than "
            "1400 is not supported); test _MSC_VER directly")
    endif()
endforeach()

# The option list of cmake/LumexOptions.cmake: no XML character option.
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _opts)
if("${_opts}" MATCHES "option\\(LUMEX_XML_[A-Z_]*(WCHAR|CHAR)")
    message(FATAL_ERROR
        "cmake/LumexOptions.cmake declares an XML character-type option")
endif()
string(FIND "${_opts}" "option(LUMEX_BUILD_XML " _build_xml)
if(_build_xml EQUAL -1)
    message(FATAL_ERROR "LUMEX_BUILD_XML must stay: only the wide mode went")
endif()

# The one place that names the character type.
file(READ "${LUMEX_SOURCE_DIR}/lumex/xml/types/XmlTypes.hpp" _types)
string(FIND "${_types}" "using char_t = char;" _char_pos)
if(_char_pos EQUAL -1)
    message(FATAL_ERROR "XmlTypes.hpp: char_t must be the plain alias of char")
endif()
if("${_types}" MATCHES "lumex_wstring_view[^\n]*;")
    message(FATAL_ERROR
        "XmlTypes.hpp: string_view_t must be lumex_string_view only")
endif()
