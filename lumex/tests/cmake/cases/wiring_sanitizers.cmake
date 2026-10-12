# Sanitizer wiring: CMakeRoutines configure_sanitizers, default OFF,
# same flags on libraries, test executables, and vendored gtest.

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

_require_option_default_off(LUMEX_USE_ASAN)
_require_option_default_off(LUMEX_USE_UBSAN)
_require_option_default_off(LUMEX_USE_TSAN)

_require_text("cmake/LumexBuild.cmake" "include(testing/SanitizersConfig)")
_require_text("cmake/LumexBuild.cmake" "lumex_apply_sanitizers")
_require_text("cmake/LumexBuild.cmake" "configure_sanitizers")
_require_text("cmake/LumexBuild.cmake" "_configure_clang_sanitizers")
_require_text("cmake/LumexBuild.cmake" "ADDRESS")
_require_text("cmake/LumexBuild.cmake" "UNDEFINED")
_require_text("cmake/LumexBuild.cmake" "THREAD")
_require_text("cmake/LumexBuild.cmake" "lumex_gtest_1_12")
_require_text("cmake/LumexBuild.cmake" "lumex_gtest_1_18")
_require_text("cmake/LumexBuild.cmake" "lumex_gtest_main_1_12")
_require_text("cmake/LumexBuild.cmake" "lumex_gtest_main_1_18")
_require_text("cmake/LumexBuild.cmake"
    "LUMEX_USE_ASAN and LUMEX_USE_TSAN cannot be used together")
_require_text("cmake/LumexBuild.cmake"
    "LUMEX_USE_TSAN is not supported on Windows")

_require_text("cmake/LumexBuild.cmake" "LumexSanitizerBuildType.cmake")
_require_text("cmake/LumexSanitizerBuildType.cmake"
    "lumex_warn_sanitizers_without_debug_info")
_require_text("cmake/LumexSanitizerBuildType.cmake" "RelWithDebInfo")
_require_text("cmake/LumexSanitizerBuildType.cmake"
    "Debug is the recommended configuration")
_require_text("cmake/LumexSanitizerBuildType.cmake"
    "lumex_apply_asan_runtime_library")
_require_text("cmake/LumexSanitizerBuildType.cmake" "MultiThreadedDLL")
_require_text("CMakeLists.txt" "lumex_apply_asan_runtime_library")
_require_text("CMakeLists.txt" "lumex_configure_all_compiled_targets")
_require_text("CMakeLists.txt" "lumex_warn_sanitizers_without_debug_info")
_require_text("CMakeLists.txt" "LUMEX_USE_ASAN")
_require_text("CMakeLists.txt" "LUMEX_USE_UBSAN")
_require_text("CMakeLists.txt" "LUMEX_USE_TSAN")
