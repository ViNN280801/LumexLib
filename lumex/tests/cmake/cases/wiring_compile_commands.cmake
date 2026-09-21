# CompileCommandsConfig: always ON; symlink OFF; ALL copy to repo root.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("cmake/LumexBuild.cmake" "include(utils/CompileCommandsConfig)")
_require_text("CMakeLists.txt" "configure_compile_commands")
_require_text("CMakeLists.txt" "CREATE_SYMLINK OFF")
_require_text("CMakeLists.txt" "lumex_copy_compile_commands")
_require_text("CMakeLists.txt" "CopyCompileCommandsIfPresent.cmake")

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
string(FIND "${_root}" "configure_compile_commands(ENABLE ON CREATE_SYMLINK OFF)"
    _cfg)
if(_cfg EQUAL -1)
    message(FATAL_ERROR
        "Root must call configure_compile_commands(ENABLE ON CREATE_SYMLINK OFF)")
endif()

if(NOT EXISTS
    "${LUMEX_SOURCE_DIR}/cmake/CopyCompileCommandsIfPresent.cmake")
    message(FATAL_ERROR
        "cmake/CopyCompileCommandsIfPresent.cmake is missing")
endif()
