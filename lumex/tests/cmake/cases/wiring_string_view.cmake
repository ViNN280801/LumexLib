# core/string_view wiring for the modules that take the lumex_string_view in
# their string overloads below C++17 (base64). The
# members of lumex_string_view that a call converts through are compiled into
# the string_view library (the constructors from char const * and
# std::string), and the class is dllimport on Windows, so a module that has
# the view in its API links the module: the configure-time edge, the link of
# lumex::string_view, the Conan requirement and the order of the subdirectories
# (the alias target must exist when the module links it).

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
foreach(_user BASE64)
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
# The modules that take the view: the edge, the link and the Conan component.
foreach(_user BASE64)
    _require_text("cmake/LumexModules.cmake"
        "lumex_require_module(LUMEX_BUILD_${_user} LUMEX_BUILD_STRING_VIEW)")
endforeach()
_require_text("lumex/core/base64/CMakeLists.txt" "lumex::string_view")
# The Conan components list core_string_view among their requirements.
file(READ "${LUMEX_SOURCE_DIR}/conanfile.py" _conan)
foreach(_component "core_base64")
    string(REGEX MATCH "\"${_component}\"[^)]*\"core_string_view\"" _found
           "${_conan}")
    if(NOT _found)
        message(FATAL_ERROR
            "conanfile.py: ${_component} does not require core_string_view")
    endif()
endforeach()
