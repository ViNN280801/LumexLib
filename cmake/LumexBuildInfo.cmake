# cmake/LumexBuildInfo.cmake
#
# lumex_write_build_info(<CMakeRoutines utils dir> <source dir> <binary dir>
#                        <python> <format> <output dir> <library files...>)
#
# The CMakeRoutines build info of each shipped library (LUMEX_GENERATE_BUILD_INFO):
# compiler, flags, standards, runtime, optimizations, git state, host and the
# size, hashes and dependencies of the binary itself, written as
# <output dir>/<target>-build-info.<format>. Runs CMakeRoutines'
# GenerateBuildInfoScript.cmake on the finished file, with the context the
# configure step wrote into <binary dir> (<target>-build-info-context.json,
# <target>-target-properties.cmake).
#
# A POST_BUILD step cannot do it: lumex_configure_target runs from the root
# directory for targets of the subdirectories, and add_custom_command (TARGET)
# only accepts a target of the calling directory. So cmake/PublishDistr.cmake
# and cmake/InstallBuildInfo.cmake call this over the files they ship. A file
# that is not a Lumex library of this build is skipped; a failure is a
# warning, never a failed build.

function(lumex_write_build_info utils_dir source_dir binary_dir python format output_dir)
  set(_script "${utils_dir}/GenerateBuildInfoScript.cmake")
  if(NOT EXISTS "${_script}")
    message(WARNING "LumexBuildInfo: ${_script} is missing; no build info written.")
    return()
  endif()
  string(TOLOWER "${format}" format)
  foreach(_file IN LISTS ARGN)
    if(IS_SYMLINK "${_file}" OR NOT EXISTS "${_file}")
      continue()
    endif()
    get_filename_component(_name "${_file}" NAME)
    # Lumex<Module>.dll, libLumex<Module>.so[.<version>], libLumex<Module>.dylib
    if(NOT _name MATCHES "^(lib)?(Lumex[A-Za-z0-9_]+)\\.(dll|so|dylib)")
      continue()
    endif()
    set(_target "${CMAKE_MATCH_2}")
    if(NOT EXISTS "${binary_dir}/${_target}-build-info-context.json")
      continue()
    endif()
    # publish_distr runs on every build: a report newer than its binary is
    # current (a change of flags relinks the binary; the context file is
    # rewritten by every configure and is no measure).
    set(_report "${output_dir}/${_target}-build-info.${format}")
    if(EXISTS "${_report}" AND NOT "${_file}" IS_NEWER_THAN "${_report}")
      continue()
    endif()
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
              "-Dtarget_name=${_target}"
              "-Doutput_file=${_report}"
              "-Dtarget_properties_file=${binary_dir}/${_target}-target-properties.cmake"
              "-DCMAKE_SOURCE_DIR=${source_dir}"
              "-DCMAKE_BINARY_DIR=${binary_dir}"
              "-Dpython_executable=${python}"
              "-Dpython_script=${utils_dir}/GenerateBuildInfo.py"
              "-Dbuild_info_format=${format}"
              "-Dtarget_binary_path=${_file}"
              -P "${_script}"
      RESULT_VARIABLE _rc
      OUTPUT_VARIABLE _out
      ERROR_VARIABLE _out)
    if(_rc EQUAL 0)
      message(STATUS "LumexBuildInfo: ${_report}")
    else()
      message(WARNING "LumexBuildInfo: no build info for ${_name} (${_rc}):\n${_out}")
    endif()
  endforeach()
endfunction()
