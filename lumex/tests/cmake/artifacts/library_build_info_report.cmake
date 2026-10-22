# Run by cmake.build_info_report (a tree with LUMEX_GENERATE_BUILD_INFO, a
# shared LumexCore_utility and Python 3).
#
# cmake/LumexBuildInfo.cmake, as publish_distr and the install hook run it, on
# a library of this very tree: the report is the JSON of CMakeRoutines'
# GenerateBuildInfo.py (not the CMake-native text it falls back to), it
# describes that library, and its SHA-256 is the one of the file. A second
# call leaves a current report alone (publish_distr runs on every build), and
# a newer binary gets a new report.
#
# Required: PYTHON, LUMEX_SOURCE_DIR, BINARY_DIR (the build tree), WORK_DIR,
# LIBRARY (the library file).

foreach(_var PYTHON LUMEX_SOURCE_DIR BINARY_DIR WORK_DIR LIBRARY)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR "build_info_report: ${_var} is required")
    endif()
endforeach()

include("${LUMEX_SOURCE_DIR}/cmake/LumexBuildInfo.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(COPY "${LIBRARY}" DESTINATION "${WORK_DIR}")
get_filename_component(_name "${LIBRARY}" NAME)
set(_file "${WORK_DIR}/${_name}")
string(REGEX REPLACE "^(lib)?(Lumex[A-Za-z0-9_]+)\\..*$" "\\2" _target "${_name}")
set(_report "${WORK_DIR}/${_target}-build-info.json")

function(_write)
    lumex_write_build_info("${LUMEX_SOURCE_DIR}/CMakeRoutines/utils"
        "${LUMEX_SOURCE_DIR}" "${BINARY_DIR}" "${PYTHON}" json "${WORK_DIR}"
        "${_file}")
endfunction()

_write()
if(NOT EXISTS "${_report}")
    message(FATAL_ERROR "build_info_report: ${_report} was not written")
endif()
file(READ "${_report}" _json)
string(JSON _generator ERROR_VARIABLE _err GET "${_json}" meta generator)
if(_err OR NOT _generator STREQUAL "GenerateBuildInfo.py")
    message(FATAL_ERROR "build_info_report: not the JSON of GenerateBuildInfo.py:\n${_json}")
endif()
string(JSON _sha ERROR_VARIABLE _err GET "${_json}" binary sha256)
file(SHA256 "${_file}" _expected)
if(_err OR NOT _sha STREQUAL _expected)
    message(FATAL_ERROR
        "build_info_report: SHA-256 ${_sha} in the report, ${_expected} of ${_name}")
endif()
if(NOT _json MATCHES "\"${_target}\"")
    message(FATAL_ERROR "build_info_report: the report does not name ${_target}")
endif()

# Current report: left alone.
file(TIMESTAMP "${_report}" _first "%Y%m%d%H%M%S" UTC)
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 2)
_write()
file(TIMESTAMP "${_report}" _second "%Y%m%d%H%M%S" UTC)
if(NOT _first STREQUAL _second)
    message(FATAL_ERROR "build_info_report: a current report was written again")
endif()

# Newer binary: a new report.
execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${_file}")
_write()
file(TIMESTAMP "${_report}" _third "%Y%m%d%H%M%S" UTC)
if(_third STREQUAL _second)
    message(FATAL_ERROR "build_info_report: the report of a newer binary was not renewed")
endif()
message(STATUS "build_info_report: ${_target} reported, kept while current, renewed after a relink")
