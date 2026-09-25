# lumex/tests/applied/logger/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexLoggerTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexLogger.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::logger
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
lumex_gtest_discover_tests(${TEST_EXECUTABLE_NAME})
