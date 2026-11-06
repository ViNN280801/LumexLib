# compile.py --cmake-args takes ';' and ':' as separators, and the release
# script of Windows passes -DLUMEX_DOCUMENTATION_PREBUILT_DIR=D:/... through it:
# the colon of a drive letter must stay in the path. compile.py needs colorlog
# and psutil at import, so the fixture runs only _split_extra_cmake_args, once
# with each os.pathsep.

find_program(_python NAMES python3 python)
if(NOT _python)
    message(STATUS "Skipping compile_script_cmake_args: no Python 3 interpreter")
    return()
endif()

set(_checker "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/fixtures/compile_py/check_split_cmake_args.py")
foreach(_pathsep ":" ";")
    execute_process(
        COMMAND ${_python} ${_checker} ${LUMEX_SOURCE_DIR}/compile.py "${_pathsep}"
        RESULT_VARIABLE _code
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err)
    if(NOT _code EQUAL 0)
        message(FATAL_ERROR "split of --cmake-args with os.pathsep '${_pathsep}' failed (${_code}):\n${_out}${_err}")
    endif()
endforeach()
