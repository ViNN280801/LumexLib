# Configures, builds and runs one consumer fixture under consumer/<CASE>.
#
# Required: CASE, LUMEX_SOURCE_DIR, WORK_DIR, GENERATOR, CXX_COMPILER.
# Optional: BUILD_TYPE (default Release), EXTRA_ARGS (;-list of -D...),
# CONFIGURE_ONLY (stop after a successful configure).
#
# The nested build uses the compiler environment of the calling ctest. On
# MSVC that means a developer shell (INCLUDE/LIB set); without it the case
# prints LUMEX_CONSUMER_SKIP and CTest reports it as skipped.

foreach(_var CASE LUMEX_SOURCE_DIR WORK_DIR GENERATOR CXX_COMPILER)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR "run_consumer.cmake: ${_var} is required")
    endif()
endforeach()
if(NOT BUILD_TYPE)
    set(BUILD_TYPE Release)
endif()

get_filename_component(_compiler_name "${CXX_COMPILER}" NAME_WE)
string(TOLOWER "${_compiler_name}" _compiler_name)
if(WIN32 AND (_compiler_name STREQUAL "cl" OR _compiler_name STREQUAL "clang-cl")
   AND "$ENV{INCLUDE}" STREQUAL "")
    message("LUMEX_CONSUMER_SKIP: no MSVC environment (INCLUDE is empty); "
            "run ctest from a developer shell")
    return()
endif()

set(_source "${CMAKE_CURRENT_LIST_DIR}/${CASE}")
set(_binary "${WORK_DIR}/${CASE}")
file(REMOVE_RECURSE "${_binary}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${_source}" -B "${_binary}" -G "${GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
        "-DLUMEX_SOURCE_DIR=${LUMEX_SOURCE_DIR}"
        ${EXTRA_ARGS}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "consumer ${CASE}: configure failed (${_rc})\n${_out}")
endif()

# Fixtures that check everything while configuring (try_compile) stop here;
# their output is echoed so CTest can match LUMEX_CONSUMER_SKIP.
if(CONFIGURE_ONLY)
    message("${_out}")
    message(STATUS "consumer ${CASE}: configure OK")
    return()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_binary}" --config "${BUILD_TYPE}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "consumer ${CASE}: build failed (${_rc})\n${_out}")
endif()

# Every fixture builds one executable named <CASE>_consumer.
file(GLOB_RECURSE _exe LIST_DIRECTORIES false
    "${_binary}/${CASE}_consumer"
    "${_binary}/${CASE}_consumer.exe"
    "${_binary}/*/${CASE}_consumer.exe")
list(REMOVE_DUPLICATES _exe)
list(LENGTH _exe _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "consumer ${CASE}: executable not found under ${_binary}")
endif()
list(GET _exe 0 _exe)
get_filename_component(_exe_dir "${_exe}" DIRECTORY)

# Shared Lumex libraries land in the embedded build tree; put every
# directory that holds one on PATH so the executable can start on Windows.
file(GLOB_RECURSE _dlls LIST_DIRECTORIES false "${_binary}/*.dll")
set(_path_dirs "${_exe_dir}")
foreach(_dll ${_dlls})
    get_filename_component(_d "${_dll}" DIRECTORY)
    list(APPEND _path_dirs "${_d}")
endforeach()
list(REMOVE_DUPLICATES _path_dirs)
if(WIN32)
    list(JOIN _path_dirs ";" _joined)
    set(ENV{PATH} "${_joined};$ENV{PATH}")
else()
    list(JOIN _path_dirs ":" _joined)
    set(ENV{PATH} "${_joined}:$ENV{PATH}")
endif()

execute_process(
    COMMAND "${_exe}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "consumer ${CASE}: executable exited with ${_rc} "
        "(reflection_umbrella: 2 = LUMEX_WITH_FIELD_REFLECTION leaked into the consumer)\n${_out}")
endif()
message(STATUS "consumer ${CASE}: configure, build and run OK")
