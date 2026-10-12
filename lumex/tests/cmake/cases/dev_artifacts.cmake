# The dev/ folder of an installed package: the link-time files and the debug
# symbols a developer needs - the import .lib, the .exp and the PDB on MSVC
# (cl.exe and clang-cl), the .debug the ELF linker launcher splits out
# otherwise. The rules live in cmake/LumexModules.cmake and the root file
# must call them under LUMEX_INSTALL; a missing string here is a package that
# a developer cannot link against or debug.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("CMakeLists.txt" "lumex_install_dev_artifacts()")
_require_text("cmake/LumexModules.cmake" "function(lumex_install_dev_artifacts)")
_require_text("cmake/LumexModules.cmake" "DESTINATION dev OPTIONAL")
_require_text("cmake/LumexModules.cmake" "TARGET_LINKER_FILE")
_require_text("cmake/LumexModules.cmake" "TARGET_PDB_FILE")
_require_text("cmake/LumexModules.cmake" ">.exp")
_require_text("cmake/LumexModules.cmake" ">.debug")
