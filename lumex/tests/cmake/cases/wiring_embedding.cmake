# LumexLib must stay embeddable with add_subdirectory() from a parent project:
# no compiler/vcvars work, no forced parent cache, no writes into this source
# tree, no parent-rooted paths, install rules behind LUMEX_INSTALL.

function(_read_normalized path out_var)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(REPLACE "\r" "" _txt "${_txt}")
    set(${out_var} "${_txt}" PARENT_SCOPE)
endfunction()

function(_require_snippet path needle)
    _read_normalized("${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not contain:\n${needle}")
    endif()
endfunction()

# Root CMakeLists.txt: top-level detection happens before project().
_read_normalized("CMakeLists.txt" _root)
string(FIND "${_root}" "if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)\n    set(LUMEX_IS_TOP_LEVEL TRUE)" _top)
string(FIND "${_root}" "project(LumexLib" _proj)
if(_top EQUAL -1 OR _proj EQUAL -1 OR _top GREATER _proj)
    message(FATAL_ERROR "LUMEX_IS_TOP_LEVEL must be computed before project(LumexLib)")
endif()

# Own CMakeRoutines first on the module path; appending would let a parent's
# checkout shadow it.
_require_snippet("CMakeLists.txt"
    "list(INSERT CMAKE_MODULE_PATH 0 \"\${CMAKE_CURRENT_SOURCE_DIR}/CMakeRoutines\")")
string(FIND "${_root}" "list(APPEND CMAKE_MODULE_PATH \"\${CMAKE_CURRENT_SOURCE_DIR}/CMakeRoutines\")" _append)
if(NOT _append EQUAL -1)
    message(FATAL_ERROR "CMakeRoutines must be prepended, not appended, to CMAKE_MODULE_PATH")
endif()

# vcvars only when top level (it FATALs after the parent's project()).
string(FIND "${_root}" "if(NOT LUMEX_IS_TOP_LEVEL)" _skip)
string(FIND "${_root}" "include(core/MsvcVcvarsConfig)" _inc)
if(_skip EQUAL -1 OR _inc EQUAL -1 OR _skip GREATER _inc)
    message(FATAL_ERROR "configure_msvc_vcvars() must be skipped when LumexLib is embedded")
endif()

# Parent-owned cache and parent-owned outputs.
_require_snippet("CMakeLists.txt"
    "if(LUMEX_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)")
_require_snippet("CMakeLists.txt"
    "if(LUMEX_IS_TOP_LEVEL)\n    configure_compile_commands(ENABLE ON CREATE_SYMLINK OFF)")
_require_snippet("CMakeLists.txt"
    "if(LUMEX_IS_TOP_LEVEL)\n    lumex_add_publish_distr_target()")
_require_snippet("CMakeLists.txt"
    "if(LUMEX_INSTALL)\n    lumex_install_export_if_any(")

# LUMEX_INSTALL: declared, defaults to the top-level flag.
_require_snippet("cmake/LumexOptions.cmake" "option(LUMEX_INSTALL ")
_require_snippet("cmake/LumexOptions.cmake"
    "if(LUMEX_IS_TOP_LEVEL)\n    set(_lumex_install_default ON)")

# No path rooted at the parent's source tree.
file(GLOB_RECURSE _lists RELATIVE "${LUMEX_SOURCE_DIR}"
    "${LUMEX_SOURCE_DIR}/lumex/core/*CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/applied/*CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/xml/*CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/tests/*CMakeLists.txt")
list(APPEND _lists
    "CMakeLists.txt"
    "lumex/CMakeLists.txt"
    "cmake/LumexNlohmannJson.cmake"
    "cmake/LumexBuild.cmake")
set(_needle "\${CMAKE_SOURCE_DIR}")
foreach(_f IN LISTS _lists)
    _read_normalized("${_f}" _txt)
    string(FIND "${_txt}" "${_needle}" _p)
    if(NOT _p EQUAL -1)
        message(FATAL_ERROR
            "${_f} uses \${CMAKE_SOURCE_DIR}; use \${LumexLib_SOURCE_DIR} "
            "(or a path resolved from the file) so an embedding parent works")
    endif()
endforeach()

# Every module install() sits behind LUMEX_INSTALL: no install( in column 0.
file(GLOB_RECURSE _modules RELATIVE "${LUMEX_SOURCE_DIR}"
    "${LUMEX_SOURCE_DIR}/lumex/core/*CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/applied/*CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/xml/*CMakeLists.txt"
    "${LUMEX_SOURCE_DIR}/lumex/examples/*CMakeLists.txt")
list(APPEND _modules "lumex/CMakeLists.txt")
foreach(_f IN LISTS _modules)
    _read_normalized("${_f}" _txt)
    if("\n${_txt}" MATCHES "\ninstall\\(")
        message(FATAL_ERROR "${_f} has an install() outside if(LUMEX_INSTALL)")
    endif()
    string(FIND "${_txt}" "install(" _has_install)
    string(FIND "${_txt}" "LUMEX_INSTALL" _has_guard)
    if(NOT _has_install EQUAL -1 AND _has_guard EQUAL -1)
        message(FATAL_ERROR "${_f} installs files without checking LUMEX_INSTALL")
    endif()
endforeach()
