# cmake/InstallSourceRevision.cmake
#
# install(SCRIPT) hook of the top-level packaging (root CMakeLists.txt, under
# LUMEX_INSTALL), registered last: writes LumexLib-source-revision.rev at the
# root of the installed tree, listing the INSTALLED libraries (bin/ on
# Windows, lib/ elsewhere) and their symbols in dev/ (cmake/LumexSourceRevision.cmake).
# The release packages are made from this tree, so each carries the commits
# it was built from and the symbol keys of its binaries.
#
# Set before the include by install(CODE): LUMEX_SOURCE_REVISION_PYTHON (the
# interpreter found at configure time) and LUMEX_SOURCE_REVISION_SOURCE_DIR.

get_filename_component(_lumex_rev_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${_lumex_rev_dir}/LumexSourceRevision.cmake")

set(_lumex_rev_prefix "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
if(WIN32)
  set(_lumex_rev_binaries "${_lumex_rev_prefix}/bin")
else()
  if(NOT CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib")
  endif()
  set(_lumex_rev_binaries "${_lumex_rev_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

lumex_write_source_revision(
  "${LUMEX_SOURCE_REVISION_PYTHON}"
  "${LUMEX_SOURCE_REVISION_SOURCE_DIR}"
  "${_lumex_rev_prefix}/LumexLib-source-revision.rev"
  "${_lumex_rev_binaries}"
  "${_lumex_rev_prefix}/dev")
