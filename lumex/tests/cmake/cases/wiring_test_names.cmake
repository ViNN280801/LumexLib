# CTest names follow one scheme: <prefix><name>, where the prefix is derived
# from the directory that registers the test (cmake/LumexTestNames.cmake), so
# that `ctest -R '^<module>\.'` selects a module and every subdirectory under
# it. The path under lumex/tests loses a leading core/ or applied/ group, "/"
# becomes "." and a "." is appended; examples use lumex/ as the root and get
# examples.<module>.; an explicit TEST_PREFIX replaces the derived prefix.

include("${LUMEX_SOURCE_DIR}/cmake/LumexTestNames.cmake")

get_filename_component(_lumex "${LUMEX_SOURCE_DIR}/lumex" ABSOLUTE)
if(NOT LUMEX_TEST_NAMES_LUMEX_DIR STREQUAL _lumex)
    message(FATAL_ERROR
        "LUMEX_TEST_NAMES_LUMEX_DIR is '${LUMEX_TEST_NAMES_LUMEX_DIR}', "
        "expected '${_lumex}'")
endif()
set(_tests "${_lumex}/tests")

function(_expect_prefix directory expected)
    lumex_test_name_prefix(_got "${_tests}/${directory}" "${_tests}")
    if(NOT _got STREQUAL "${expected}")
        message(FATAL_ERROR
            "lumex/tests/${directory}: prefix '${_got}', expected '${expected}'")
    endif()
endfunction()

function(_expect_name expected name)
    lumex_test_name(_got "${name}" ${ARGN})
    if(NOT _got STREQUAL "${expected}")
        message(FATAL_ERROR
            "lumex_test_name(${name} ${ARGN}) gave '${_got}', "
            "expected '${expected}'")
    endif()
endfunction()

# The examples of the decision, one per kind of directory.
_expect_prefix("core/crc" "crc.")
_expect_prefix("core/string_view" "string_view.")
_expect_prefix("core/span" "span.")
_expect_prefix("core/hazard_pointer/holder" "hazard_pointer.holder.")
_expect_prefix("core/contracts/assert" "contracts.assert.")
_expect_prefix("core/span/view" "span.view.")
_expect_prefix("applied/json" "json.")
_expect_prefix("xml" "xml.")
_expect_prefix("core/generators/number_generator" "generators.number_generator.")
_expect_prefix("cmake" "cmake.")
_expect_prefix("core/atomic" "atomic.")
# A later split of a module directory keeps the module as the first part.
_expect_prefix("core/math/constants" "math.constants.")
# The test tree follows the source tree: one directory per source directory,
# nested ones included.
_expect_prefix("core/math/ops" "math.ops.")
_expect_prefix("core/utility/traits" "utility.traits.")
_expect_prefix("applied/json/schema" "json.schema.")
_expect_prefix("applied/resource_monitor/monitor/detail"
    "resource_monitor.monitor.detail.")
_expect_prefix("xml/xpath/query" "xml.xpath.query.")
# Only the leading group is dropped, and only as a whole path component.
_expect_prefix("xml/core" "xml.core.")
_expect_prefix("applied/serial/core" "serial.core.")
_expect_prefix("corextra" "corextra.")
_expect_prefix("applied_x/json" "applied_x.json.")
# The group directory itself and the root of the tree.
_expect_prefix("core" "core.")
_expect_prefix("" "")
# A trailing slash or a "." component does not change the result.
_expect_prefix("core/crc/" "crc.")
_expect_prefix("core/./crc" "crc.")

# Full names: the derived prefix, the explicit override (also an empty one)
# and the examples root.
_expect_name("crc.LumexCrcTests" "LumexCrcTests"
    DIRECTORY "${_tests}/core/crc")
_expect_name("generators.number_generator.LumexNumberGeneratorTests"
    "LumexNumberGeneratorTests"
    DIRECTORY "${_tests}/core/generators/number_generator")
_expect_name("cmake.wiring_test_names" "wiring_test_names"
    DIRECTORY "${_tests}/cmake")
_expect_name("crc." "" DIRECTORY "${_tests}/core/crc")
_expect_name("custom.LumexCrcTests" "LumexCrcTests"
    DIRECTORY "${_tests}/core/crc" TEST_PREFIX "custom.")
_expect_name("LumexCrcTests" "LumexCrcTests"
    DIRECTORY "${_tests}/core/crc" TEST_PREFIX "")
_expect_name("examples.atomic.LumexAtomicExample" "LumexAtomicExample"
    DIRECTORY "${_lumex}/examples/atomic" ROOT "${_lumex}")
_expect_name("examples.fmt.LumexFormatExamplesCoverage"
    "LumexFormatExamplesCoverage"
    DIRECTORY "${_lumex}/examples/fmt" ROOT "${_lumex}")

# Wiring: GoogleTest discovery passes the derived prefix unless the caller
# gives one, and the other registrations name tests through the same helper.
function(_require_text path needle)
    file(READ "${LUMEX_SOURCE_DIR}/${path}" _txt)
    string(FIND "${_txt}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${path} does not mention ${needle}")
    endif()
endfunction()

set(_gtest "cmake/LumexGoogleTest.cmake")
_require_text("${_gtest}"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/LumexTestNames.cmake\")")
_require_text("${_gtest}"
    "cmake_parse_arguments(ARG \"\" \"TEST_PREFIX\" \"\" \${ARGN})")
_require_text("${_gtest}" "set(_override TEST_PREFIX \"\${ARG_TEST_PREFIX}\")")
_require_text("${_gtest}" "lumex_test_name(_prefix \"\" \${_override})")
_require_text("${_gtest}" "TEST_PREFIX \"\${_prefix}\"")
_require_text("${_gtest}" "\${ARG_UNPARSED_ARGUMENTS}")

_require_text("lumex/tests/cmake/CMakeLists.txt"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/../../../cmake/LumexTestNames.cmake\")")
_require_text("lumex/examples/cmake/LumexExampleHelpers.cmake"
    "include(\"\${_LUMEX_EXAMPLE_HELPERS_DIR}/../../../cmake/LumexTestNames.cmake\")")
_require_text("lumex/examples/cmake/LumexExampleHelpers.cmake"
    "lumex_test_name(_name \"\${name}\" ROOT \"\${LUMEX_TEST_NAMES_LUMEX_DIR}\")")
_require_text("lumex/examples/cmake/LumexExampleHelpers.cmake"
    "lumex_example_test_name(_ctest_name \${LEX_NAME})")
_require_text("lumex/examples/fmt/CMakeLists.txt"
    "lumex_example_test_name(_ctest_name LumexFormatExamplesCoverage)")

# Every add_test of the test and example trees names its test through
# lumex_test_name or lumex_example_test_name (the variable _ctest_name), or
# is a lint.* check registered at the top of lumex/tests. The consumer
# fixtures are separate projects and register no tests of this tree.
file(GLOB_RECURSE _lists
    "${_lumex}/tests/CMakeLists.txt"
    "${_lumex}/examples/CMakeLists.txt"
    "${_lumex}/examples/*.cmake")
set(_bad "")
foreach(_list IN LISTS _lists)
    if(_list MATCHES "/tests/cmake/consumer/")
        continue()
    endif()
    file(STRINGS "${_list}" _lines)
    set(_n 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _n "${_n} + 1")
        if(_line MATCHES "^[ \t]*#" OR NOT _line MATCHES "(^|[^_A-Za-z0-9])add_test\\(")
            continue()
        endif()
        if(NOT _line MATCHES "add_test\\(NAME (\\\${_ctest_name}|lint\\.[a-z_]+)([ \t]|$)")
            string(APPEND _bad "  ${_list}:${_n}: ${_line}\n")
        endif()
    endforeach()
endforeach()
if(_bad)
    message(FATAL_ERROR
        "add_test must use a name from lumex_test_name / "
        "lumex_example_test_name (\${_ctest_name}) or lint.<check>:\n${_bad}")
endif()
