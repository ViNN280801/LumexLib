# core/math/constants wiring: every LUMEX_MATH_CONSTANTS_* macro of the header
# has a row in the reference table of the tests (so a constant added to the
# header without a reference value, or a row of a macro that is gone, fails
# here), the table says how many rows it has, the test files and the shared
# headers of the directory exist at their standards, and the negative
# compile-check fixture (math_constants_compile_checks) is registered with
# its good case and as many bad cases as check.cpp defines.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

set(_header "lumex/core/math/constants/LumexMathConstants.hpp")
set(_tests "lumex/tests/core/math/constants")

# The macros of the header and the rows of the table.
file(READ "${LUMEX_SOURCE_DIR}/${_header}" _header_text)
string(REGEX MATCHALL "#define LUMEX_MATH_CONSTANTS_[A-Z0-9_]+" _defines
    "${_header_text}")
set(_macros "")
foreach(_define IN LISTS _defines)
    string(REPLACE "#define LUMEX_MATH_CONSTANTS_" "" _name "${_define}")
    list(APPEND _macros "${_name}")
endforeach()
list(LENGTH _macros _macro_count)

file(READ "${LUMEX_SOURCE_DIR}/${_tests}/LumexMathConstantsReference.hpp"
    _table_text)
string(REGEX MATCHALL "\n  X \\([A-Z0-9_]+," _rows "${_table_text}")
set(_row_names "")
foreach(_row IN LISTS _rows)
    string(REGEX REPLACE "^\n  X \\(([A-Z0-9_]+),$" "\\1" _name "${_row}")
    list(APPEND _row_names "${_name}")
endforeach()
list(LENGTH _row_names _row_count)

list(SORT _macros)
list(SORT _row_names)
if(NOT "${_macros}" STREQUAL "${_row_names}")
    message(FATAL_ERROR
        "the macros of ${_header} and the rows of "
        "${_tests}/LumexMathConstantsReference.hpp differ:\n"
        "  header: ${_macros}\n  table:  ${_row_names}")
endif()
_require_text("${_tests}/LumexMathConstantsReference.hpp"
    "#define LUMEX_TEST_MATH_CONSTANTS_COUNT ${_macro_count}")

# The files of the directory, at the standards the suites build.
foreach(_file
        LumexMathConstantsDigits.cxx11.tests.cpp
        LumexMathConstantsValues.cxx11.tests.cpp
        LumexMathConstantsRelations.cxx11.tests.cpp
        LumexMathConstantsMacros.cxx11.tests.cpp
        LumexMathConstantsSpecialFunctions.cxx17.tests.cpp
        LumexMathConstantsStdNumbers.cxx20.tests.cpp
        LumexMathConstantsReference.hpp
        LumexMathConstantsReference.py
        LumexMathConstantsSupport.hpp
        CMakeLists.txt)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/${_tests}/${_file}")
        message(FATAL_ERROR "${_tests} has no ${_file}")
    endif()
endforeach()
_require_text("${_tests}/CMakeLists.txt" "MODULE math")
_require_text("${_tests}/CMakeLists.txt" "LumexMathConstantsReference.hpp")
_require_text("${_tests}/CMakeLists.txt" "LumexMathConstantsSupport.hpp")
_require_text("lumex/tests/core/math/CMakeLists.txt" "add_subdirectory(constants)")

# The negative compile checks, run by the consumer runner at configure time.
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name math_constants_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "-DCASE=math_constants_compile_checks")
set(_fixture "lumex/tests/cmake/consumer/math_constants_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS "${LUMEX_SOURCE_DIR}/${_fixture}/${_file}")
        message(FATAL_ERROR "math_constants_compile_checks has no ${_file}")
    endif()
endforeach()
_require_text("${_fixture}/CMakeLists.txt" "set(_standards 11 17 20)")
file(READ "${LUMEX_SOURCE_DIR}/${_fixture}/CMakeLists.txt" _fixture_cmake)
string(REGEX MATCH "set\\(_last_case ([0-9]+)\\)" _last "${_fixture_cmake}")
set(_last_case "${CMAKE_MATCH_1}")
file(READ "${LUMEX_SOURCE_DIR}/${_fixture}/check.cpp" _check_text)
string(REGEX MATCHALL "LUMEX_MATH_CONSTANTS_BAD_CASE == [0-9]+" _cases
    "${_check_text}")
list(LENGTH _cases _case_count)
if(NOT _case_count EQUAL _last_case)
    message(FATAL_ERROR
        "check.cpp defines ${_case_count} bad cases, the CMake file runs "
        "${_last_case}")
endif()
