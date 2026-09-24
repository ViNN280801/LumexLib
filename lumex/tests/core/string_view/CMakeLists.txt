# lumex/tests/core/string_view/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexStringViewTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexStringView.tests.cpp
    LumexStringViewImplicit.tests.cpp
    LumexWStringView.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::string_view
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})
