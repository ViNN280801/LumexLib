# The include-order gate (quality-gates section 7) runs as the CTest
# LumexIncludeOrder whenever tests are built, so a new violation fails the
# suite instead of waiting for someone to run the checker by hand. The test
# scans the whole lumex/ tree with --check and is registered next to the
# LumexCMake cases, outside any module group.

file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/CMakeLists.txt" _tests)

string(FIND "${_tests}" "add_test(NAME LumexIncludeOrder" _add_pos)
if(_add_pos EQUAL -1)
    message(FATAL_ERROR
        "lumex/tests/CMakeLists.txt does not register LumexIncludeOrder")
endif()

string(REGEX MATCH
    "add_test\\(NAME LumexIncludeOrder[^)]*check_include_order\\.py[^)]*--check[^)]*--dir [^)]*/lumex[^)]*\\)"
    _call "${_tests}")
if(_call STREQUAL "")
    message(FATAL_ERROR
        "LumexIncludeOrder must run Scripts/CodeTools/check_include_order.py "
        "--check over the lumex/ tree")
endif()

# Registered at the top of the tests tree, not inside a module group that
# could be switched off.
string(FIND "${_tests}" "lumex_add_subdirectory_if(" _group_pos)
if(_group_pos EQUAL -1 OR NOT _add_pos LESS _group_pos)
    message(FATAL_ERROR
        "LumexIncludeOrder must be registered before the module groups in "
        "lumex/tests/CMakeLists.txt")
endif()

if(NOT EXISTS "${LUMEX_SOURCE_DIR}/Scripts/CodeTools/check_include_order.py")
    message(FATAL_ERROR "Scripts/CodeTools/check_include_order.py is missing")
endif()
