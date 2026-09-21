# cmake -P helper. Ninja / Makefile generators write compile_commands.json
# under the build tree. Visual Studio and Xcode do not, even when
# CMAKE_EXPORT_COMPILE_COMMANDS is ON. Missing source must not fail ALL.

if(NOT SOURCE)
  message(FATAL_ERROR "CopyCompileCommandsIfPresent: SOURCE is required")
endif()
if(NOT DEST)
  message(FATAL_ERROR "CopyCompileCommandsIfPresent: DEST is required")
endif()

if(NOT EXISTS "${SOURCE}")
  return()
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SOURCE}" "${DEST}"
  RESULT_VARIABLE _lumex_cc_copy_rc
)
if(_lumex_cc_copy_rc)
  message(FATAL_ERROR
    "CopyCompileCommandsIfPresent: copy failed (${_lumex_cc_copy_rc})")
endif()
