# The udev rules of the transport (G7) go into share/LumexLib/udev with the
# transport's install, and the permission_denied log hint names exactly that
# place: a hint that points at a source-tree path is useless to a user of the
# installed package.

set(_dir lumex/examples/transport/udev)
foreach(_file 99-lumex-serial.rules 70-lumex-uaccess.rules README.md)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/${_dir}/${_file}")
        message(FATAL_ERROR "${_dir}/${_file} is missing")
    endif()
endforeach()

# The uaccess tag is acted on by 73-seat-late.rules: the file sorts below it.
file(READ "${LUMEX_SOURCE_DIR}/${_dir}/70-lumex-uaccess.rules" _uaccess)
if(NOT _uaccess MATCHES "TAG\\+=\"uaccess\"")
    message(FATAL_ERROR "70-lumex-uaccess.rules does not set TAG+=\"uaccess\"")
endif()

file(READ "${LUMEX_SOURCE_DIR}/lumex/applied/transport/CMakeLists.txt" _cmake)
string(REGEX MATCH
       "install\\([ \t\r\n]*DIRECTORY[^)]*examples/transport/udev/[^)]*DESTINATION[ \t\r\n]*\\$\\{CMAKE_INSTALL_DATADIR\\}/\\$\\{PROJECT_NAME\\}/udev[^)]*\\)"
       _install "${_cmake}")
if(_install STREQUAL "")
    message(FATAL_ERROR
        "lumex/applied/transport/CMakeLists.txt does not install"
        " examples/transport/udev into CMAKE_INSTALL_DATADIR/PROJECT_NAME/udev")
endif()

file(READ
     "${LUMEX_SOURCE_DIR}/lumex/applied/transport/detail/LumexPermissionDiagnostics.cpp"
     _diagnostics)
if(NOT _diagnostics MATCHES "LumexLib udev rule \\(share/LumexLib/udev\\)")
    message(FATAL_ERROR
        "the permission hint does not point at share/LumexLib/udev")
endif()
if(_diagnostics MATCHES "examples/transport/udev")
    message(FATAL_ERROR "the permission hint names a source-tree path")
endif()
