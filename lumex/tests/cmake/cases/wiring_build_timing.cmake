# BuildTiming: option default OFF; root calls configure_build_timing.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_require_option_default_off name)
    file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _opts)
    string(REGEX MATCH "option\\(${name}[^\n]*\\)" _line "${_opts}")
    if(NOT _line)
        message(FATAL_ERROR
            "cmake/LumexOptions.cmake has no option(${name} ...)")
    endif()
    if(NOT _line MATCHES " OFF\\)$")
        message(FATAL_ERROR
            "${name} default is not OFF: ${_line}")
    endif()
endfunction()

_require_option_default_off(LUMEX_BUILD_TIMING)

_require_text("cmake/LumexBuild.cmake" "include(utils/BuildTiming)")
_require_text("CMakeLists.txt" "configure_build_timing")
_require_text("CMakeLists.txt" "LUMEX_BUILD_TIMING")

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
string(FIND "${_root}" "include(cmake/LumexBuild.cmake)" _inc)
string(FIND "${_root}" "configure_build_timing" _cfg)
if(_inc EQUAL -1 OR _cfg EQUAL -1 OR _cfg LESS _inc)
    message(FATAL_ERROR
        "configure_build_timing() must come after include(cmake/LumexBuild.cmake)")
endif()
