# The CPack wiring the Windows release script drives (create_release.ps1
# -Formats exe): compile.py -i runs cpack after a Release build, so the root
# file must configure a generator and keep the package version and the install
# directory on project(). A missing string here is a packaging hole.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("CMakeLists.txt" "if(LUMEX_IS_TOP_LEVEL AND LUMEX_INSTALL)")
_require_text("CMakeLists.txt" "include(CPack)")
_require_text("CMakeLists.txt" "set(CPACK_PACKAGE_NAME \"LumexLib\")")
_require_text("CMakeLists.txt" "set(CPACK_PACKAGE_VERSION \"\${PROJECT_VERSION}\")")
_require_text("CMakeLists.txt" "set(CPACK_PACKAGE_FILE_NAME \"LumexLib-\${PROJECT_VERSION}-win\")")
_require_text("CMakeLists.txt" "set(CPACK_PACKAGE_INSTALL_DIRECTORY \"LumexLib/\${PROJECT_VERSION}\"")
_require_text("CMakeLists.txt" "set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY \"LumexLib \${PROJECT_VERSION}\"")
_require_text("CMakeLists.txt" "set(CPACK_NSIS_DISPLAY_NAME \"LumexLib \${PROJECT_VERSION}\"")
_require_text("CMakeLists.txt" "set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)")
_require_text("CMakeLists.txt" "set(CPACK_GENERATOR \"NSIS\")")
_require_text("CMakeLists.txt" "set(CPACK_GENERATOR \"TGZ\")")
