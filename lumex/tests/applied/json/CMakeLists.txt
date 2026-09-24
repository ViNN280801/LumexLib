# lumex/tests/applied/json/CMakeLists.txt
#
# lumex::json is header-only and compiles against the consumer's nlohmann;
# the tests use the vendored 3rdparty/nlohmann copy.

lumex_setup_nlohmann_json()

set(LUMEX_JSON_TEST_SOURCES
    LumexJsonDiagnostics.tests.cpp
    LumexJsonHelper.tests.cpp
    LumexJsonSchema.tests.cpp
)

set(TEST_EXECUTABLE_NAME LumexJsonTests)

add_executable(${TEST_EXECUTABLE_NAME} ${LUMEX_JSON_TEST_SOURCES})

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::json
    nlohmann_json::nlohmann_json
)

# The helper tests write files named after the suite, so this suite and the
# C++20 one below can run in parallel.
target_compile_definitions(${TEST_EXECUTABLE_NAME}
    PRIVATE LUMEX_JSON_TEST_SUITE="cxx11")

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})

# Same sources at C++20: consumers such as PeakExpertWeb compile these headers
# at C++20, and the schema walker takes its std::ranges branch there.
# TEST_SUFFIX keeps CTest names distinct.
set(JSON_CXX20_TEST_NAME LumexJsonCxx20Tests)

add_executable(${JSON_CXX20_TEST_NAME} ${LUMEX_JSON_TEST_SOURCES})

set_target_properties(${JSON_CXX20_TEST_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${JSON_CXX20_TEST_NAME}
    PRIVATE
    lumex::json
    nlohmann_json::nlohmann_json
)

target_compile_definitions(${JSON_CXX20_TEST_NAME}
    PRIVATE LUMEX_JSON_TEST_SUITE="cxx20")

lumex_test_use_gtest(${JSON_CXX20_TEST_NAME} CXX_STANDARD 20)
gtest_discover_tests(${JSON_CXX20_TEST_NAME} TEST_SUFFIX ".cxx20")
