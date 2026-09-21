# lumex/tests/core/math/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexMathTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexMath.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::math
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 20)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})
