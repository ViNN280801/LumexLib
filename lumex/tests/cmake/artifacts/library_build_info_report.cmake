# Run by cmake.build_info_report (a tree with LUMEX_GENERATE_BUILD_INFO, shared
# LumexCore_utility and LumexCore_contracts, Python 3).
#
# cmake/LumexBuildInfo.cmake, as publish_distr and the install hook run it, on
# two libraries of this very tree: one LumexLib-build-info.json holds the
# common sections once (the report of CMakeRoutines' GenerateBuildInfo.py, not
# the CMake-native text it falls back to) and a "libraries" list with each
# library's target and binary; the SHA-256 of each is the one of the file. A
# second call leaves a current report alone (publish_distr runs on every
# build), and a newer library gets a new report.
#
# Required: PYTHON, LUMEX_SOURCE_DIR, BINARY_DIR (the build tree), WORK_DIR,
# LIBRARY, OTHER_LIBRARY (two library files).

foreach(_var PYTHON LUMEX_SOURCE_DIR BINARY_DIR WORK_DIR LIBRARY OTHER_LIBRARY)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR "build_info_report: ${_var} is required")
    endif()
endforeach()

include("${LUMEX_SOURCE_DIR}/cmake/LumexBuildInfo.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/bin")
file(COPY "${LIBRARY}" "${OTHER_LIBRARY}" DESTINATION "${WORK_DIR}/bin")
file(GLOB _files "${WORK_DIR}/bin/*")
set(_report "${WORK_DIR}/LumexLib-build-info.json")

function(_write)
    lumex_write_build_info("${LUMEX_SOURCE_DIR}/CMakeRoutines/utils"
        "${LUMEX_SOURCE_DIR}" "${BINARY_DIR}" "${PYTHON}" "${_report}" ${_files})
endfunction()

_write()
if(NOT EXISTS "${_report}")
    message(FATAL_ERROR "build_info_report: ${_report} was not written")
endif()
file(READ "${_report}" _json)
string(JSON _generator ERROR_VARIABLE _err GET "${_json}" meta generator)
if(_err OR NOT _generator STREQUAL "GenerateBuildInfo.py")
    message(FATAL_ERROR "build_info_report: not the report of GenerateBuildInfo.py:\n${_json}")
endif()
foreach(_common compiler cmake git platform tools)
    string(JSON _type ERROR_VARIABLE _err TYPE "${_json}" ${_common})
    if(_err OR NOT _type STREQUAL "OBJECT")
        message(FATAL_ERROR "build_info_report: no common section ${_common}")
    endif()
endforeach()
string(JSON _top ERROR_VARIABLE _err TYPE "${_json}" target)
if(NOT _err)
    message(FATAL_ERROR "build_info_report: a per-library section at the top level")
endif()
string(JSON _count ERROR_VARIABLE _err LENGTH "${_json}" libraries)
if(_err OR NOT _count EQUAL 2)
    message(FATAL_ERROR "build_info_report: ${_count} libraries listed, 2 expected:\n${_json}")
endif()
math(EXPR _last "${_count} - 1")
foreach(_i RANGE 0 ${_last})
    string(JSON _name GET "${_json}" libraries ${_i} target name)
    string(JSON _path GET "${_json}" libraries ${_i} binary path)
    string(JSON _sha GET "${_json}" libraries ${_i} binary sha256)
    get_filename_component(_file_name "${_path}" NAME)
    if(NOT _file_name MATCHES "^(lib)?${_name}\\.")
        message(FATAL_ERROR "build_info_report: target ${_name} listed with ${_file_name}")
    endif()
    file(SHA256 "${_path}" _expected)
    if(NOT _sha STREQUAL _expected)
        message(FATAL_ERROR "build_info_report: SHA-256 ${_sha} listed, ${_expected} of ${_file_name}")
    endif()
endforeach()

# Current report: left alone.
file(TIMESTAMP "${_report}" _first "%Y%m%d%H%M%S" UTC)
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 2)
_write()
file(TIMESTAMP "${_report}" _second "%Y%m%d%H%M%S" UTC)
if(NOT _first STREQUAL _second)
    message(FATAL_ERROR "build_info_report: a current report was written again")
endif()

# One newer library: a new report.
list(GET _files 0 _touched)
execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${_touched}")
_write()
file(TIMESTAMP "${_report}" _third "%Y%m%d%H%M%S" UTC)
if(_third STREQUAL _second)
    message(FATAL_ERROR "build_info_report: the report was not renewed after a relink")
endif()
message(STATUS "build_info_report: one report, ${_count} libraries, kept while current, renewed after a relink")
