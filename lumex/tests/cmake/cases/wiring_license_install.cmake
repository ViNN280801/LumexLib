# The license and the third-party notices (pugixml, from which lumex/xml is
# derived) go with every copy: CMake installs both into share/LumexLib, and
# the Conan recipe exports both and copies them into the package licenses.

set(_files LICENSE THIRD-PARTY-NOTICES.md)

foreach(_file IN LISTS _files)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/${_file}")
        message(FATAL_ERROR "${_file} is missing from the repository root")
    endif()
endforeach()

file(READ "${LUMEX_SOURCE_DIR}/THIRD-PARTY-NOTICES.md" _notices)
if(NOT _notices MATCHES "Arseny Kapoulkine")
    message(FATAL_ERROR "THIRD-PARTY-NOTICES.md lacks the pugixml notice")
endif()

file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
string(REGEX MATCH
       "install\\([ \t\r\n]*FILES[^)]*/LICENSE\"[^)]*/THIRD-PARTY-NOTICES\\.md\"[^)]*CMAKE_INSTALL_DATADIR[^)]*\\)"
       _install "${_root}")
if(_install STREQUAL "")
    message(FATAL_ERROR
        "CMakeLists.txt does not install LICENSE and THIRD-PARTY-NOTICES.md"
        " into CMAKE_INSTALL_DATADIR")
endif()

file(READ "${LUMEX_SOURCE_DIR}/conanfile.py" _conan)
foreach(_file IN LISTS _files)
    string(FIND "${_conan}" "\"${_file}\"" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "conanfile.py does not name ${_file}")
    endif()
endforeach()
if(NOT _conan MATCHES "for name in \\(\"LICENSE\", \"THIRD-PARTY-NOTICES.md\"\\)")
    message(FATAL_ERROR "conanfile.py package() does not copy both files")
endif()
