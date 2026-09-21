# CMakeRoutines deployment/BuildInfoPrinter is included in LumexBuild
# and invoked once after lumex_configure_all_compiled_targets.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("cmake/LumexBuild.cmake" "include(deployment/BuildInfoPrinter)")
_require_text("CMakeLists.txt" "print_build_info()")
_require_text("CMakeLists.txt" "lumex_configure_all_compiled_targets()")

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
string(FIND "${_root}" "lumex_configure_all_compiled_targets()" _cfg)
string(FIND "${_root}" "print_build_info()" _print)
if(_cfg EQUAL -1 OR _print EQUAL -1 OR _print LESS _cfg)
    message(FATAL_ERROR
        "print_build_info() must come after lumex_configure_all_compiled_targets()")
endif()
