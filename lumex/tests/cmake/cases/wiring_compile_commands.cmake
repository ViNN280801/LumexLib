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
string(REPLACE "\r" "" _root "${_root}")
string(FIND "${_root}" "configure_compile_commands(ENABLE ON CREATE_SYMLINK OFF)"
    _cfg)
if(_cfg EQUAL -1)
    message(FATAL_ERROR
        "Root must call configure_compile_commands(ENABLE ON CREATE_SYMLINK OFF)")
endif()

# EXPORT_COMPILE_COMMANDS is copied onto a target when that target is
# created. The call has to precede add_subdirectory(lumex); a later call
# leaves every library translation unit out of the first configure.
string(FIND "${_root}" "add_subdirectory(lumex)\n" _subdir)
if(_subdir EQUAL -1 OR _cfg GREATER _subdir)
    message(FATAL_ERROR
        "configure_compile_commands must run before add_subdirectory(lumex)")
endif()

if(NOT EXISTS
    "${LUMEX_SOURCE_DIR}/cmake/CopyCompileCommandsIfPresent.cmake")
    message(FATAL_ERROR
        "cmake/CopyCompileCommandsIfPresent.cmake is missing")
endif()
