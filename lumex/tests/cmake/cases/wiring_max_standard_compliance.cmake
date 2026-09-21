# MaximumStandardCompliance: default ON; only GNU/Clang/AppleClang;
# only targets that already pin CXX_STANDARD. Do not flip this option to
# OFF to silence the case - the default is a product decision.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_require_option_default_on name)
    file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _opts)
    string(REGEX MATCH "option\\(${name}[^\n]*\\)" _line "${_opts}")
    if(NOT _line)
        message(FATAL_ERROR
            "cmake/LumexOptions.cmake has no option(${name} ...)")
    endif()
    if(NOT _line MATCHES " ON\\)$")
        message(FATAL_ERROR
            "${name} default is not ON: ${_line}")
    endif()
endfunction()

_require_option_default_on(LUMEX_MAXIMUM_STANDARD_COMPLIANCE)

_require_text("cmake/LumexBuild.cmake"
    "include(analysis/MaximumStandardCompliance)")
_require_text("cmake/LumexBuild.cmake"
    "lumex_apply_maximum_standard_compliance")
_require_text("cmake/LumexBuild.cmake"
    "maximum_standard_compliance_configure")
_require_text("cmake/LumexBuild.cmake" "GNU|Clang|AppleClang")
_require_text("cmake/LumexBuild.cmake" "CXX_STANDARD")
_require_text("cmake/LumexBuild.cmake" "RELAX_VENDOR")
_require_text("cmake/LumexBuild.cmake" "WERROR OFF")
_require_text("CMakeLists.txt" "LUMEX_MAXIMUM_STANDARD_COMPLIANCE")
