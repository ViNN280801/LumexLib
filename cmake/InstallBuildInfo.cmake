# cmake/InstallBuildInfo.cmake
#
# install(SCRIPT) hook of the top-level packaging (root CMakeLists.txt, under
# LUMEX_INSTALL and LUMEX_GENERATE_BUILD_INFO): LumexLib-build-info.json at the
# root of the installed tree, next to LumexLib-source-revision.rev, over the
# INSTALLED libraries (bin/ on Windows, lib/ elsewhere); cmake/LumexBuildInfo.cmake.
#
# Set before the include by install(CODE): LUMEX_BUILD_INFO_UTILS_DIR,
# LUMEX_BUILD_INFO_SOURCE_DIR, LUMEX_BUILD_INFO_BINARY_DIR,
# LUMEX_BUILD_INFO_PYTHON.

get_filename_component(_lumex_bi_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${_lumex_bi_dir}/LumexBuildInfo.cmake")

set(_lumex_bi_prefix "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
if(WIN32)
  file(GLOB _lumex_bi_files "${_lumex_bi_prefix}/bin/Lumex*.dll")
else()
  if(NOT CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib")
  endif()
  file(GLOB _lumex_bi_files
    "${_lumex_bi_prefix}/${CMAKE_INSTALL_LIBDIR}/libLumex*.so*"
    "${_lumex_bi_prefix}/${CMAKE_INSTALL_LIBDIR}/libLumex*.dylib")
endif()

# A reinstall over an older tree: the report is rewritten, never kept.
file(REMOVE "${_lumex_bi_prefix}/LumexLib-build-info.json")
lumex_write_build_info(
  "${LUMEX_BUILD_INFO_UTILS_DIR}"
  "${LUMEX_BUILD_INFO_SOURCE_DIR}"
  "${LUMEX_BUILD_INFO_BINARY_DIR}"
  "${LUMEX_BUILD_INFO_PYTHON}"
  "${_lumex_bi_prefix}/LumexLib-build-info.json"
  ${_lumex_bi_files})
