# lumex/tests/core/string/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexStringifyTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    LumexJoin.tests.cpp
    LumexQuote.tests.cpp
    LumexString.tests.cpp
    LumexTextConstraints.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::string
    lumex::string_view
)

lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 11)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})

# Same sources at C++20: text::join / quote* switch to their std::ranges
# implementation and the stream traits switch from SFINAE to concepts. TEST_SUFFIX keeps CTest names distinct.
set(STRING_CXX20_TEST_NAME LumexStringifyCxx20Tests)

add_executable(
    ${STRING_CXX20_TEST_NAME}
    LumexJoin.tests.cpp
    LumexQuote.tests.cpp
    LumexString.tests.cpp
    LumexTextConstraints.tests.cpp
)

target_link_libraries(
    ${STRING_CXX20_TEST_NAME}
    PRIVATE
    lumex::string
    lumex::string_view
)

lumex_test_use_gtest(${STRING_CXX20_TEST_NAME} CXX_STANDARD 20)
gtest_discover_tests(${STRING_CXX20_TEST_NAME} TEST_SUFFIX ".cxx20")
