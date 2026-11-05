# The layout of the package (user, 2026-11-05): everything optional goes under
# share/ - share/doc (the documentation, with LUMEX_BUILD_DOCUMENTATION),
# share/examples and share/benchmarks (sources only, per module). share/doc and
# not share/doc/LumexLib: the prefix of a package is already one version of one
# library (/opt/LumexLib/<version>_<compiler>). The license, the notices and the
# udev rules stay in share/LumexLib. The behaviour is in
# cmake.install_share_layout_*; this case pins the pieces of the wiring.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_require_no_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} must not mention ${needle}")
    endif()
endfunction()

foreach(_name LUMEX_INSTALL_EXAMPLES LUMEX_INSTALL_BENCHMARKS)
    _require_text("cmake/LumexOptions.cmake" "option(${_name} ")
endforeach()
_require_text("cmake/LumexOptions.cmake" "LUMEX_DOCUMENTATION_OUTPUT_DIR")
_require_text("cmake/LumexOptions.cmake" "LUMEX_DOCUMENTATION_PREBUILT_DIR")

# The documentation: share/doc, from a directory of the build tree (or a
# finished one), never from the checkout.
_require_text("CMakeLists.txt" "\${CMAKE_INSTALL_DATADIR}/doc")
_require_no_text("CMakeLists.txt" "\${PROJECT_NAME}/docs")
_require_no_text("CMakeLists.txt" "set(DOXYGEN_OUTPUT_DIR \"\${LumexLib_SOURCE_DIR}/docs\")")
_require_text("Doxyfile.in" "@LUMEX_DOXYGEN_OUTPUT_DIRECTORY@")

# Examples and benchmarks: sources, under share/, by the install rules of one file.
_require_text("CMakeLists.txt" "include(cmake/LumexInstallSources.cmake)")
_require_text("CMakeLists.txt" "lumex_install_example_sources()")
_require_text("CMakeLists.txt" "lumex_install_benchmark_sources()")
_require_text("cmake/LumexInstallSources.cmake" "\${CMAKE_INSTALL_DATADIR}/examples")
_require_text("cmake/LumexInstallSources.cmake" "\${CMAKE_INSTALL_DATADIR}/benchmarks")
_require_text("cmake/LumexInstallSources.cmake" "lumex/examples/cmake")
# The udev rules are installed by the transport into share/LumexLib/udev, where
# the permission hint names them: not twice.
_require_text("cmake/LumexInstallSources.cmake" "PATTERN \"udev\" EXCLUDE")
# What a consumer cannot run from an installed package stays out: the compile
# time scripts compile with -I<checkout> (and the protocol one reads the tests).
_require_text("cmake/LumexInstallSources.cmake" "PATTERN \"compile_time.*\" EXCLUDE")

# The release scripts build the documentation into the package (or are told not to).
_require_text("create_release.sh" "--no-docs")
_require_text("create_release.sh" "-DLUMEX_BUILD_DOCUMENTATION=ON")
_require_text("create_release.sh" "-DLUMEX_DOCUMENTATION_PREBUILT_DIR=")
_require_text("create_release.sh" "-DLUMEX_BUILD_DOCUMENTATION=OFF")
_require_text("create_release.ps1" "NoDocs")
_require_text("create_release.ps1" "-DLUMEX_BUILD_DOCUMENTATION=ON")
_require_text("create_release.ps1" "-DLUMEX_BUILD_DOCUMENTATION=OFF")
_require_text("create_release.ps1" "-DLUMEX_DOCUMENTATION_PREBUILT_DIR=")
