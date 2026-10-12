# Calling configure_msvc_vcvars after project() is a FATAL_ERROR.
# run_one.cmake expects this case to fail.

list(APPEND CMAKE_MODULE_PATH "${LUMEX_SOURCE_DIR}/CMakeRoutines")
set(PROJECT_NAME LumexLib)
include(core/MsvcVcvarsConfig)
configure_msvc_vcvars()
message(FATAL_ERROR
    "configure_msvc_vcvars() should have aborted after project()")
