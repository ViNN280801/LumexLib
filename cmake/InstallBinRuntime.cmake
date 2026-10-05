# cmake/InstallBinRuntime.cmake
#
# install(SCRIPT) hook of the top-level packaging (root CMakeLists.txt, under
# LUMEX_INSTALL + WIN32): after the shared libraries are installed, stage the
# compiler runtime they link against next to them in bin/. That is the pack's
# CopyRuntimeDependencies with its default Windows name set - msvcp140.dll and
# vcruntime140*.dll - the same files publish_distr already stages into
# <platform>/Distr<Config>; a consumer machine that never installed the MSVC
# redistributable still loads the libraries. The dependency walk runs against
# the INSTALLED files, so it sees exactly what ships.
#
# Windows-only by design: on ELF the runtime handling stays with
# create_release.sh (it stages the compiler's own libstdc++/libc++ into lib/)
# and publish_distr's replacement step, and the walk here would only duplicate
# that.
if(NOT WIN32)
  return()
endif()

get_filename_component(_lumex_script_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
set(_lumex_copy_runtime "${_lumex_script_dir}/../CMakeRoutines/deployment/CopyRuntimeDependencies.cmake")
if(NOT EXISTS "${_lumex_copy_runtime}")
  message(WARNING "InstallBinRuntime: ${_lumex_copy_runtime} is missing; no runtime staged.")
  return()
endif()

if(NOT CMAKE_INSTALL_BINDIR)
  set(CMAKE_INSTALL_BINDIR "bin")
endif()
set(_lumex_bin "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
if(NOT IS_DIRECTORY "${_lumex_bin}")
  message(WARNING "InstallBinRuntime: ${_lumex_bin} does not exist; no runtime staged.")
  return()
endif()

file(GLOB _lumex_probe "${_lumex_bin}/Lumex*.dll")
if(NOT _lumex_probe)
  message(WARNING "InstallBinRuntime: no Lumex*.dll under ${_lumex_bin}; no runtime staged.")
  return()
endif()
list(GET _lumex_probe 0 target_file)
include("${_lumex_copy_runtime}")
