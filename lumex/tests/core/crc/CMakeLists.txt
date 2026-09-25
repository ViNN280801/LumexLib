# lumex/tests/core/crc/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexCrcTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexCrc.tests.cpp
    LumexCrcTestHelpers.hpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::crc
)

# Catalogue check tests use std::make_index_sequence (C++14).
lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 14)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
    lumex_gtest_discover_tests(${TEST_EXECUTABLE_NAME}
        DISCOVERY_TIMEOUT 60
        DISCOVERY_MODE PRE_TEST
    )
else()
    add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND ${TEST_EXECUTABLE_NAME})
endif()
