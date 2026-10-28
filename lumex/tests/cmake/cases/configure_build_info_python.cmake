# The root configure names the reason when LumexLib-build-info.json cannot be
# written: no Python interpreter, or one older than 3.8 (GenerateBuildInfo.py
# needs 3.8). The decision is lumex_build_info_python_problem; the root
# CMakeLists.txt turns its answer into a warning.

include("${LUMEX_SOURCE_DIR}/cmake/LumexBuildInfo.cmake")

function(_expect_problem found version expected)
    lumex_build_info_python_problem(_problem "${found}" "${version}")
    if(NOT "${_problem}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "python found=${found} version='${version}': expected "
            "'${expected}', got '${_problem}'")
    endif()
endfunction()

_expect_problem(FALSE "" "no Python 3 interpreter was found")
_expect_problem(TRUE "3.7.3" "Python 3.7.3 is older than 3.8")
_expect_problem(TRUE "3.6" "Python 3.6 is older than 3.8")
_expect_problem(TRUE "3.8.0" "")
_expect_problem(TRUE "3.10.1" "")
_expect_problem(TRUE "3.12" "")

# The root configure asks the question and warns (only with the report on).
file(READ "${LUMEX_SOURCE_DIR}/CMakeLists.txt" _root)
foreach(_needle
        "if(LUMEX_GENERATE_BUILD_INFO)"
        "lumex_build_info_python_problem(_lumex_build_info_problem"
        "LumexLib-build-info.json will not be written"
        "Python 3.8 or newer")
    string(FIND "${_root}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "CMakeLists.txt does not contain: ${_needle}")
    endif()
endforeach()
