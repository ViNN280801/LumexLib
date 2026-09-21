# lumex/tests/core/circular_buffer/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexCircularBufferTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    CircularBuffer.header.tests.cpp
    CircularBuffer.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::circular_buffer
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})
