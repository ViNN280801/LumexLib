# lumex/tests/xml/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexXmlTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexXml.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::xml
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})

target_link_options(${TEST_EXECUTABLE_NAME} PRIVATE -Wl,--no-as-needed)
