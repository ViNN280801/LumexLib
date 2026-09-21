# lumex/tests/core/expected/CMakeLists.txt

set(TEST_EXECUTABLE_NAME LumexExpectedTests)

add_executable(
    ${TEST_EXECUTABLE_NAME}
    BadExpectedAccess.tests.cpp
    Expected.tests.cpp
    ExpectedVoid.tests.cpp
    Unexpected.tests.cpp
)

target_link_libraries(
    ${TEST_EXECUTABLE_NAME}
    PRIVATE
    lumex::expected
)

# Test sources use if constexpr (C++17), but Expected.hpp / ExpectedVoid.hpp
# declare constexpr destructors, which MSVC accepts only in C++20 (C2178).
lumex_test_use_gtest(${TEST_EXECUTABLE_NAME} CXX_STANDARD 20)
gtest_discover_tests(${TEST_EXECUTABLE_NAME})
