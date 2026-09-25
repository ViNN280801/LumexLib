# lumex/tests/applied/serial/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexSerialPortTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexSerialPort.tests.cpp
    LumexSerialPortEnumeration.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::serial
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
# PRE_TEST: do not run the exe at build time. A POST_BUILD discover
# of this suite previously left cmake.exe blocked while ninja still
# held the tree (CreateFile on Bluetooth COM in an older probe).
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    lumex_gtest_discover_tests(${TEST_EXECUTABLE_NAME}
        DISCOVERY_TIMEOUT 30
        DISCOVERY_MODE PRE_TEST
    )
else()
    add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND ${TEST_EXECUTABLE_NAME})
endif()
