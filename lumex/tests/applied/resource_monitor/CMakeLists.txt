# lumex/tests/applied/resource_monitor/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexResourceMonitorTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexResourceMonitor.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::resource_monitor
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 17)
lumex_gtest_discover_tests(${TEST_EXECUTABLE_NAME})
