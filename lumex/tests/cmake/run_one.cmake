# lumex/tests/cmake/run_one.cmake
#
# cmake -DCASE=<name> -DLUMEX_SOURCE_DIR=<root> [-DCASE_EXPECT_FAIL=1]
#       [-DCASE_FAIL_PATTERN=<regex>] -P run_one.cmake

cmake_minimum_required(VERSION 3.16)

if(NOT CASE)
    message(FATAL_ERROR "CASE is required")
endif()
if(NOT LUMEX_SOURCE_DIR)
    message(FATAL_ERROR "LUMEX_SOURCE_DIR is required")
endif()

set(_case_file "${CMAKE_CURRENT_LIST_DIR}/cases/${CASE}.cmake")
if(NOT EXISTS "${_case_file}")
    message(FATAL_ERROR "missing case file: ${_case_file}")
endif()

if(CASE_EXPECT_FAIL)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DLUMEX_SOURCE_DIR=${LUMEX_SOURCE_DIR}
            -P ${_case_file}
        RESULT_VARIABLE _rv
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(_rv EQUAL 0)
        message(FATAL_ERROR
            "case ${CASE} was expected to fail but returned 0\n${_out}${_err}")
    endif()
    if(CASE_FAIL_PATTERN AND NOT "${_err}${_out}" MATCHES "${CASE_FAIL_PATTERN}")
        message(FATAL_ERROR
            "case ${CASE} failed, but the output did not match '${CASE_FAIL_PATTERN}'\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()
else()
    include("${_case_file}")
endif()
