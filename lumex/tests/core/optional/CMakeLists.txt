# lumex/tests/core/optional/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexOptionalTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexOptional.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::optional
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
lumex_gtest_discover_tests(${TEST_EXECUTABLE_NAME})
