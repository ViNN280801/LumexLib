# StaticAnalysisConfig: options default OFF; LumexBuild applies
# configure_static_analysis only when tidy or cppcheck is ON.

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

_require_option_default_off(LUMEX_USE_CLANG_TIDY)
_require_option_default_off(LUMEX_USE_CPPCHECK)

_require_text("cmake/LumexOptions.cmake" "LUMEX_STATIC_ANALYSIS_PROFILE")
_require_text("cmake/LumexBuild.cmake" "include(analysis/StaticAnalysisConfig)")
_require_text("cmake/LumexBuild.cmake" "lumex_apply_static_analysis")
_require_text("cmake/LumexBuild.cmake" "configure_static_analysis")
_require_text("cmake/LumexBuild.cmake" "HEADER_FILTER")
_require_text("cmake/LumexBuild.cmake" "lumex/")
_require_text("CMakeLists.txt" "LUMEX_USE_CLANG_TIDY")
_require_text("CMakeLists.txt" "LUMEX_USE_CPPCHECK")
_require_text("CMakeLists.txt" "LUMEX_STATIC_ANALYSIS_PROFILE")
