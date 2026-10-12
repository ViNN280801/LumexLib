# VersionConfig is included from LumexBuild and exercised here via cmake -P.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

_require_text("cmake/LumexBuild.cmake" "include(core/VersionConfig)")
_require_text("cmake/LumexBuild.cmake" "configure_version(VERSION")
_require_text("cmake/LumexBuild.cmake" "PROJECT_VERSION")
_require_text("cmake/LumexBuild.cmake" "LibraryVersioning.cmake")

list(APPEND CMAKE_MODULE_PATH "${LUMEX_SOURCE_DIR}/CMakeRoutines")
include(core/VersionConfig)
configure_version(VERSION 1.2.3.4)
if(NOT PROJECT_VERSION STREQUAL "1.2.3.4")
    message(FATAL_ERROR "PROJECT_VERSION is '${PROJECT_VERSION}'")
endif()
if(NOT PROJECT_VERSION_MAJOR STREQUAL "1"
        OR NOT PROJECT_VERSION_MINOR STREQUAL "2"
        OR NOT PROJECT_VERSION_PATCH STREQUAL "3"
        OR NOT PROJECT_VERSION_TWEAK STREQUAL "4")
    message(FATAL_ERROR
        "parsed version is ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}."
        "${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}")
endif()
