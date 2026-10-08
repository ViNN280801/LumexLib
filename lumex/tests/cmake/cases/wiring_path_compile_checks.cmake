# lumex::path converts to std::string only explicitly (MAJOR 2.0.0.0). The
# operator is `explicit`, no implicit one is left behind, and the negative
# compile-check fixture (path_compile_checks) is registered: its good case and
# every bad case have a source, and the number of bad cases the CMake file
# runs equals the number check.cpp defines.

function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

function(_forbid_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "${path} mentions ${needle}")
    endif()
endfunction()

set(_header "lumex/core/filesystem/fs/LumexFilesystem.hpp")
# clang-format may break the line after `explicit`, so the declaration is found
# as text with any white space between the two words.
file(READ "${LUMEX_SOURCE_DIR}/${_header}" _text)
string(REGEX MATCH "explicit[ \t\r\n]+operator string_type \\(\\) const" _explicit "${_text}")
if(NOT _explicit)
    message(FATAL_ERROR "${_header} has no explicit operator string_type")
endif()
# No implicit one: once the explicit declaration is taken out, the name is gone.
string(REGEX REPLACE "explicit[ \t\r\n]+operator string_type" "" _rest "${_text}")
string(FIND "${_rest}" "operator string_type" _implicit)
if(NOT _implicit EQUAL -1)
    message(FATAL_ERROR "${_header}: an operator string_type that is not explicit")
endif()

# The fixture: registered, and its cases agree with the CMake file.
_require_text("lumex/tests/cmake/CMakeLists.txt"
    "lumex_test_name(_ctest_name path_compile_checks)")
_require_text("lumex/tests/cmake/CMakeLists.txt" "-DCASE=path_compile_checks")
foreach(_file CMakeLists.txt check.cpp)
    if(NOT EXISTS
       "${LUMEX_SOURCE_DIR}/lumex/tests/cmake/consumer/path_compile_checks/${_file}")
        message(FATAL_ERROR "path_compile_checks has no ${_file}")
    endif()
endforeach()
set(_fixture "lumex/tests/cmake/consumer/path_compile_checks")
_require_text("${_fixture}/CMakeLists.txt" "set(_standards 11 17 20)")
_require_text("${_fixture}/check.cpp" "LUMEX_PATH_GOOD_CASE")
_require_text("${_fixture}/check.cpp" "LUMEX_PATH_BAD_CASE")
file(READ "${LUMEX_SOURCE_DIR}/${_fixture}/CMakeLists.txt" _fixture_cmake)
string(REGEX MATCH "set\\(_last_case ([0-9]+)\\)" _unused "${_fixture_cmake}")
set(_last_case "${CMAKE_MATCH_1}")
file(READ "${LUMEX_SOURCE_DIR}/${_fixture}/check.cpp" _check)
string(REGEX MATCHALL "#elif LUMEX_PATH_BAD_CASE == [0-9]+" _cases "${_check}")
list(LENGTH _cases _defined)
if(NOT _last_case STREQUAL "7" OR NOT _defined EQUAL 7)
    message(FATAL_ERROR
        "path_compile_checks: the CMake file runs ${_last_case} cases, "
        "check.cpp defines ${_defined} (expected 7 of 7)")
endif()
