# core/string_view wiring for the modules that take the lumex_string_view in
# their string overloads below C++17 (base64, crc, exceptions, xml). The module
# is header-only (an INTERFACE target, no source file): a module that has the
# view in its API links lumex::string_view for the include directory, so the
# configure-time edge, the link of lumex::string_view, the Conan requirement
# and the order of the subdirectories (the alias target must exist when the
# module links it) are still checked. The module itself needs nothing else:
# no edge to utility, no compiled file, no export macro.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

# The order of the add_subdirectory calls: string_view before its users.
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/CMakeLists.txt" _core)
string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_STRING_VIEW string_view)"
       _string_view_at)
if(_string_view_at EQUAL -1)
    message(FATAL_ERROR "lumex/core/CMakeLists.txt does not add string_view")
endif()
foreach(_user BASE64 CRC EXCEPTIONS)
    string(TOLOWER "${_user}" _dir)
    string(FIND "${_core}" "lumex_add_subdirectory_if(LUMEX_BUILD_${_user} ${_dir})"
           _user_at)
    if(_user_at EQUAL -1)
        message(FATAL_ERROR "lumex/core/CMakeLists.txt does not add ${_dir}")
    endif()
    if(_user_at LESS _string_view_at)
        message(FATAL_ERROR
            "${_dir} is added before string_view, which it links")
    endif()
endforeach()
# xml is added by lumex/CMakeLists.txt after the whole core directory.
file(READ "${LUMEX_SOURCE_DIR}/lumex/CMakeLists.txt" _lumex)
string(FIND "${_lumex}" "lumex_add_subdirectory_if(LUMEX_BUILD_CORE core)" _core_at)
string(FIND "${_lumex}" "lumex_add_subdirectory_if(LUMEX_BUILD_XML xml)" _xml_at)
if(_core_at EQUAL -1 OR _xml_at LESS _core_at)
    message(FATAL_ERROR "xml must be added after core, which holds string_view")
endif()

# The modules that take the view: the edge, the link and the Conan component.
foreach(_user BASE64 CRC EXCEPTIONS XML)
    _require_text("cmake/LumexModules.cmake"
        "lumex_require_module(LUMEX_BUILD_${_user} LUMEX_BUILD_STRING_VIEW)")
endforeach()
_require_text("lumex/core/base64/CMakeLists.txt" "lumex::string_view")
_require_text("lumex/core/crc/CMakeLists.txt" "lumex::string_view")
_require_text("lumex/core/exceptions/CMakeLists.txt" "lumex::string_view")
_require_text("lumex/xml/CMakeLists.txt" "lumex::string_view")
# The Conan components list core_string_view among their requirements.
file(READ "${LUMEX_SOURCE_DIR}/conanfile.py" _conan)
foreach(_component "core_base64" "core_crc" "core_exceptions" "xml\", \"xml")
    string(REGEX MATCH "\"${_component}\"[^)]*\"core_string_view\"" _found
           "${_conan}")
    if(NOT _found)
        message(FATAL_ERROR
            "conanfile.py: ${_component} does not require core_string_view")
    endif()
endforeach()

# Header-only: an INTERFACE library, no source files, no edge to utility.
file(READ "${LUMEX_SOURCE_DIR}/lumex/core/string_view/CMakeLists.txt" _own)
if(NOT _own MATCHES "add_library\\(\\$\\{LUMEX_STRING_VIEW_NAME\\} INTERFACE\\)")
    message(FATAL_ERROR "core/string_view must be an INTERFACE library")
endif()
if(_own MATCHES "GLOB" OR _own MATCHES "lumex::utility")
    message(FATAL_ERROR "core/string_view lists sources or links utility")
endif()
file(GLOB_RECURSE _compiled "${LUMEX_SOURCE_DIR}/lumex/core/string_view/*.cpp")
if(_compiled)
    message(FATAL_ERROR "core/string_view has a compiled part: ${_compiled}")
endif()
file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake" _modules)
if(_modules MATCHES "lumex_require_module\\(LUMEX_BUILD_STRING_VIEW ")
    message(FATAL_ERROR "string_view must not require another module")
endif()
if(NOT _conan MATCHES "_component\\(\"core_string_view\", \"string_view\"\\)")
    message(FATAL_ERROR
        "conanfile.py: core_string_view must be a component without libs and requires")
endif()
