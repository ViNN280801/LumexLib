# lumex/tests/core/temporary/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexTemporaryTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexTemporary.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::temporary
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
lumex_gtest_discover_tests(${TEST_EXECUTABLE_NAME})
