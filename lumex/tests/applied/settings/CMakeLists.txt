# lumex/tests/applied/settings/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexSettingsTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexSettingsGuard.tests.cpp
    LumexSettingsINI.tests.cpp
    LumexSettingsJSON.tests.cpp
    LumexSettingsXML.tests.cpp
)

set_target_properties(${TEST_EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::filesystem
    lumex::settings
    lumex::utility
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})
