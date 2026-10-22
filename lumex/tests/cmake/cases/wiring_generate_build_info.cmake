# GenerateBuildInfo: option default ON. The configure step writes the context
# per Lumex library (LumexBuild); publish_distr and the install hook write the
# report of each shipped binary (LumexBuildInfo), into Distr<Config> and the
# packages' dev/.

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

_require_option_default_on(LUMEX_GENERATE_BUILD_INFO)
_require_text("cmake/LumexBuildInfo.cmake" "GenerateBuildInfoScript.cmake")
_require_text("cmake/LumexBuildInfo.cmake" "GenerateBuildInfo.py")
_require_text("cmake/PublishDistr.cmake" "lumex_write_build_info")
_require_text("cmake/InstallBuildInfo.cmake" "lumex_write_build_info")
_require_text("CMakeLists.txt" "cmake/InstallBuildInfo.cmake")

_require_text("cmake/LumexOptions.cmake" "LUMEX_BUILD_INFO_FORMAT")
_require_text("cmake/LumexBuild.cmake" "include(utils/GenerateBuildInfo)")
_require_text("cmake/LumexBuild.cmake" "lumex_apply_build_info")
_require_text("cmake/LumexBuild.cmake" "generate_build_info_file")
_require_text("CMakeLists.txt" "LUMEX_GENERATE_BUILD_INFO")
_require_text("CMakeLists.txt" "LUMEX_BUILD_INFO_FORMAT")

file(READ "${LUMEX_SOURCE_DIR}/cmake/LumexOptions.cmake" _opts)
string(FIND "${_opts}" "LUMEX_BUILD_INFO_FORMAT \"json\"" _json_def)
if(_json_def EQUAL -1)
    message(FATAL_ERROR
        "LUMEX_BUILD_INFO_FORMAT default must be \"json\" (PublishDistr stages *.json)")
endif()
