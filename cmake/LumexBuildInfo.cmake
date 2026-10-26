# cmake/LumexBuildInfo.cmake
#
# lumex_write_build_info(<CMakeRoutines utils dir> <source dir> <binary dir>
#                        <python> <output file> <library files...>)
#
# LumexLib-build-info.json (LUMEX_GENERATE_BUILD_INFO): one build report for
# all the shipped libraries. The common part (compiler, flags, standards,
# runtime, optimizations, git state, host, tools) comes once from CMakeRoutines'
# GenerateBuildInfo.py; "libraries" lists, per library, its target (name,
# options, definitions, link libraries) and its binary (size, SHA-256,
# SHA-512). Of the per-target reports of CMakeRoutines only these two sections
# differ between the libraries of one build, so one file holds the same
# information. Written by Scripts/ReleaseTools/write_lumex_build_info.py over
# the finished files, with the context the configure step wrote into
# <binary dir> (<target>-build-info-context.json).
#
# A POST_BUILD step cannot do it: lumex_configure_target runs from the root
# directory for targets of the subdirectories, and add_custom_command (TARGET)
# only accepts a target of the calling directory. So cmake/PublishDistr.cmake
# and cmake/InstallBuildInfo.cmake call this over the files they ship. The
# report is kept while it is newer than every library; a failure is a warning,
# never a failed build.

function(lumex_write_build_info utils_dir source_dir binary_dir python output)
  set(_script "${source_dir}/Scripts/ReleaseTools/write_lumex_build_info.py")
  if(NOT python OR NOT EXISTS "${python}")
    message(WARNING "LumexBuildInfo: no Python interpreter; ${output} not written.")
    return()
  endif()
  if(NOT EXISTS "${_script}" OR NOT EXISTS "${utils_dir}/GenerateBuildInfo.py")
    message(WARNING "LumexBuildInfo: ${_script} or ${utils_dir}/GenerateBuildInfo.py is missing; ${output} not written.")
    return()
  endif()
  set(_files)
  set(_current TRUE)
  if(NOT EXISTS "${output}")
    set(_current FALSE)
  endif()
  foreach(_file IN LISTS ARGN)
    if(IS_SYMLINK "${_file}" OR NOT EXISTS "${_file}")
      continue()
    endif()
    list(APPEND _files "${_file}")
    # publish_distr runs on every build: a report newer than every library is
    # current (a change of flags relinks the library; the context files are
    # rewritten by every configure and are no measure).
    if(_current AND "${_file}" IS_NEWER_THAN "${output}")
      set(_current FALSE)
    endif()
  endforeach()
  if(NOT _files OR _current)
    return()
  endif()
  execute_process(
    COMMAND "${python}" "${_script}"
            --utils-dir "${utils_dir}"
            --source-dir "${source_dir}"
            --binary-dir "${binary_dir}"
            --output "${output}"
            ${_files}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out)
  if(_rc EQUAL 0)
    message(STATUS "LumexBuildInfo: ${output}")
  else()
    message(WARNING "LumexBuildInfo: ${output} not written (${_rc}):\n${_out}")
  endif()
endfunction()
