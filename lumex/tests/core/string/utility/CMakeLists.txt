# lumex/tests/core/string/utility/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexStringifyTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexStringify.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::string
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})
