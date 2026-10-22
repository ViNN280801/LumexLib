# cmake/LumexSourceRevision.cmake
#
# lumex_write_source_revision(<python> <source dir> <output file>
#                             <binaries dir> <symbols dir>)
#
# Writes LumexLib-source-revision.rev: the commit of LumexLib and of every
# submodule (dirty marked), and for every binary of <binaries dir> its size,
# SHA-256 and the keys a debugger matches its symbols by - the PE time stamp
# and image size and the .pdb name, GUID and age on Windows, the Build ID and
# the .gnu_debuglink file on ELF - with whether <symbols dir> holds those very
# symbols. A crash dump records the same keys, so the file says which build a
# dump comes from and which symbols read it.
#
# The writer is Scripts/ReleaseTools/generate_git_source_revision.py, a
# byte-identical copy of PeakExpertCE's and PeakExpertWeb's script (the same
# listing in the products and in the library). Used in script mode by
# cmake/PublishDistr.cmake and cmake/InstallSourceRevision.cmake; a missing
# Python or script is a warning, never a failed build.

function(lumex_write_source_revision python source_dir output binaries symbols)
  set(_script "${source_dir}/Scripts/ReleaseTools/generate_git_source_revision.py")
  if(NOT python OR NOT EXISTS "${python}")
    message(WARNING "LumexSourceRevision: no Python interpreter; ${output} not written.")
    return()
  endif()
  if(NOT EXISTS "${_script}")
    message(WARNING "LumexSourceRevision: ${_script} is missing; ${output} not written.")
    return()
  endif()
  execute_process(
    COMMAND "${python}" "${_script}"
            --start-dir "${source_dir}"
            --sidecar-out "${output}"
            --binaries-dir "${binaries}"
            --symbols-dir "${symbols}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out)
  if(_rc EQUAL 0)
    message(STATUS "LumexSourceRevision: ${output}")
  else()
    message(WARNING "LumexSourceRevision: ${output} not written (${_rc}):\n${_out}")
  endif()
endfunction()
