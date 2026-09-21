# lumex/tests/core/base64/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexBase64Tests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexBase64Decoder.tests.cpp
    LumexBase64Encoder.tests.cpp
    LumexBase64Validator.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::base64
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})
