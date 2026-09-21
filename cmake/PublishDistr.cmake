# cmake/PublishDistr.cmake
#
# POST_BUILD cmake -P script. Mirrors PeakExpertCE PublishDistr for a
# library tree: copy Lumex shared libraries (and matching PDB / soname
# / build-info) into <platform>/Distr<Config>. Compiler CRT is staged
# into that same folder via CopyRuntimeDependencies so bin/ stays clean
# for in-tree test linking.
#
# Parameters (-D on the cmake command line):
#   bin_dir              - REQUIRED. Directory that holds built Lumex DLLs/SOs
#   distr_dir            - REQUIRED. Destination folder (Distr<Config>)
#   copy_runtime_script  - OPTIONAL. Path to CopyRuntimeDependencies.cmake
#
# Never copies test or example binaries (*Tests*, *Example*).

cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0207)
  cmake_policy(SET CMP0207 NEW)
endif()

if(NOT bin_dir OR bin_dir STREQUAL "")
  message(WARNING "PublishDistr: bin_dir not set; skipping.")
  return()
endif()

if(NOT EXISTS "${bin_dir}")
  message(WARNING "PublishDistr: bin_dir does not exist: '${bin_dir}'.")
  return()
endif()

if(NOT distr_dir OR distr_dir STREQUAL "")
  message(WARNING "PublishDistr: distr_dir not set; skipping.")
  return()
endif()

file(MAKE_DIRECTORY "${distr_dir}")

function(_lumex_distr_is_library_artifact out_var path)
  get_filename_component(_name "${path}" NAME)
  if(_name MATCHES "Tests" OR _name MATCHES "Example" OR _name MATCHES "gtest")
    set(${out_var} FALSE PARENT_SCOPE)
    return()
  endif()
  if(_name MATCHES "\\.(exe|ilk|lib|a)$")
    set(${out_var} FALSE PARENT_SCOPE)
    return()
  endif()
  set(${out_var} TRUE PARENT_SCOPE)
endfunction()

set(_to_copy "")

if(WIN32)
  file(GLOB _cands
    "${bin_dir}/Lumex*.dll"
    "${bin_dir}/Lumex*.pdb"
    "${bin_dir}/Lumex*-build-info.json"
  )
else()
  file(GLOB _cands
    "${bin_dir}/libLumex*.so"
    "${bin_dir}/libLumex*.so.*"
    "${bin_dir}/libLumex*.dylib"
    "${bin_dir}/Lumex*-build-info.json"
    "${bin_dir}/libLumex*.debug"
  )
endif()

foreach(_src IN LISTS _cands)
  _lumex_distr_is_library_artifact(_keep "${_src}")
  if(_keep)
    list(APPEND _to_copy "${_src}")
  endif()
endforeach()

list(REMOVE_DUPLICATES _to_copy)

function(_lumex_distr_copy_one _src)
  if(NOT EXISTS "${_src}" AND NOT IS_SYMLINK "${_src}")
    return()
  endif()
  get_filename_component(_name "${_src}" NAME)
  get_filename_component(_real "${_src}" REALPATH)
  if(NOT EXISTS "${_real}")
    message(WARNING "PublishDistr: dangling path '${_src}' (skipping)")
    return()
  endif()
  if(IS_SYMLINK "${_src}")
    file(READ_SYMLINK "${_src}" _link_tgt)
    if(NOT IS_ABSOLUTE "${_link_tgt}")
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "${_link_tgt}" "${distr_dir}/${_name}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE _err
        OUTPUT_QUIET
      )
      if(_rc EQUAL 0)
        message(STATUS "PublishDistr: ${_name} -> ${_link_tgt} -> ${distr_dir}")
        set(_lumex_distr_copied_local 1 PARENT_SCOPE)
        return()
      endif()
    endif()
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_real}" "${distr_dir}/${_name}"
    RESULT_VARIABLE _rc
    ERROR_VARIABLE _err
    OUTPUT_QUIET
  )
  if(_rc EQUAL 0)
    message(STATUS "PublishDistr: ${_name} -> ${distr_dir}")
    set(_lumex_distr_copied_local 1 PARENT_SCOPE)
  else()
    message(WARNING "PublishDistr: failed to copy '${_src}': ${_err}")
    set(_lumex_distr_copied_local 0 PARENT_SCOPE)
  endif()
endfunction()

set(_copied 0)
foreach(_src IN LISTS _to_copy)
  set(_lumex_distr_copied_local 0)
  _lumex_distr_copy_one("${_src}")
  if(_lumex_distr_copied_local)
    math(EXPR _copied "${_copied} + 1")
  endif()
endforeach()

message(STATUS "PublishDistr: ${_copied} file(s) -> ${distr_dir}")

if(_copied EQUAL 0)
  return()
endif()

if(NOT copy_runtime_script OR NOT EXISTS "${copy_runtime_script}")
  return()
endif()

if(WIN32)
  file(GLOB _runtime_probe "${distr_dir}/Lumex*.dll")
else()
  file(GLOB _runtime_probe "${distr_dir}/libLumex*.so")
endif()

if(NOT _runtime_probe)
  return()
endif()

list(GET _runtime_probe 0 target_file)
include("${copy_runtime_script}")
