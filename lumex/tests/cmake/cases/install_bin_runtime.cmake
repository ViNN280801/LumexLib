# The MSVC runtime next to the installed libraries (release 1.0.3.1, the
# user's re-cut: bin/ must carry the system runtimes). A Windows release
# package must load on a machine that never installed the redistributable: the
# root file registers an install(SCRIPT) hook and the script stages the pack's
# CopyRuntimeDependencies file set into bin/ - the same set
# <platform>/Distr<Config> already carries. A missing string here is a package
# that only runs where the MSVC redistributable happens to be installed.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("CMakeLists.txt" "install(SCRIPT")
_require_text("CMakeLists.txt" "cmake/InstallBinRuntime.cmake")
_require_text("cmake/InstallBinRuntime.cmake" "if(NOT WIN32)")
_require_text("cmake/InstallBinRuntime.cmake" "CopyRuntimeDependencies.cmake")
_require_text("cmake/InstallBinRuntime.cmake" "Lumex*.dll")
