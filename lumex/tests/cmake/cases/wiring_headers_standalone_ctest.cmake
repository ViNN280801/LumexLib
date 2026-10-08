# The standalone-header gate runs as the CTest lint.headers_standalone whenever
# tests are built, so a header that stops compiling alone fails the suite
# instead of waiting for someone to run the checker by hand. It must run the
# script over the whole lumex/ tree with --check at C++11 with the compiler
# of this build, carry the label lint, and be registered at the top of the
# tests tree, outside any module group that could be switched off.

file(READ "${LUMEX_SOURCE_DIR}/lumex/tests/CMakeLists.txt" _tests)

string(REGEX MATCH "add_test\\(NAME lint\\.headers_standalone[ \t\r\n]" _add "${_tests}")
string(FIND "${_tests}" "add_test(NAME lint.headers_standalone" _add_pos)
if(_add STREQUAL "" OR _add_pos EQUAL -1)
    message(FATAL_ERROR
        "lumex/tests/CMakeLists.txt does not register lint.headers_standalone")
endif()

string(REGEX MATCH
    "add_test\\(NAME lint\\.headers_standalone[^)]*check_headers_standalone\\.py[^)]*--check[^)]*--dir [^)]*/lumex[^)]*--compiler \\$\\{CMAKE_CXX_COMPILER\\}[^)]*--std 11"
    _call "${_tests}")
if(_call STREQUAL "")
    message(FATAL_ERROR
        "lint.headers_standalone must run Scripts/CodeTools/check_headers_standalone.py "
        "--check over the lumex/ tree at --std 11 with the compiler of the build")
endif()

# The C++ flags of the build, and the standard library the Lumex targets use.
string(FIND "${_tests}" "--cxx-flags=\${CMAKE_CXX_FLAGS}" _flags_pos)
if(_flags_pos EQUAL -1)
    message(FATAL_ERROR
        "lint.headers_standalone must pass the CMAKE_CXX_FLAGS of the build")
endif()
string(FIND "${_tests}" "$<FILTER:$<TARGET_PROPERTY:lumex_gtest_1_12,COMPILE_OPTIONS>,INCLUDE,^-stdlib=>"
       _stdlib_pos)
if(_stdlib_pos EQUAL -1)
    message(FATAL_ERROR
        "lint.headers_standalone must pass the -stdlib= option of the build "
        "(the one the vendored GoogleTest target carries)")
endif()

string(REGEX MATCH
    "set_tests_properties\\(lint\\.headers_standalone PROPERTIES[^)]*LABELS \"lint\"[^)]*SKIP_RETURN_CODE 77"
    _props "${_tests}")
if(_props STREQUAL "")
    message(FATAL_ERROR
        "lint.headers_standalone must carry the label lint and SKIP_RETURN_CODE 77")
endif()

string(FIND "${_tests}" "lumex_add_subdirectory_if(" _group_pos)
if(_group_pos EQUAL -1 OR NOT _add_pos LESS _group_pos)
    message(FATAL_ERROR
        "lint.headers_standalone must be registered before the module groups in "
        "lumex/tests/CMakeLists.txt")
endif()

if(NOT EXISTS "${LUMEX_SOURCE_DIR}/Scripts/CodeTools/check_headers_standalone.py")
    message(FATAL_ERROR "Scripts/CodeTools/check_headers_standalone.py is missing")
endif()
