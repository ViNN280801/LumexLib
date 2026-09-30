# lumex/tests/core/expected/CMakeLists.txt

set(LUMEX_EXPECTED_TEST_SOURCES
    BadExpectedAccess.tests.cpp
    Expected.tests.cpp
    ExpectedVoid.tests.cpp
    SuccessFailure.tests.cpp
    Unexpected.tests.cpp
)

# The test sources use if constexpr, so C++17 is their floor. C++17 is also
# the default of GCC and Clang, where LUMEX_CONSTEXPR_DTOR must stay empty
# (constexpr destructors are C++20); the C++20 suite takes the constexpr
# destructor branch of Expected.hpp / ExpectedVoid.hpp.
foreach(_expected_suite IN ITEMS
        "LumexExpectedTests;20;"
        "LumexExpectedCxx17Tests;17;.cxx17")
    list(GET _expected_suite 0 _expected_name)
    list(GET _expected_suite 1 _expected_std)
    list(GET _expected_suite 2 _expected_suffix)

    add_executable(${_expected_name} ${LUMEX_EXPECTED_TEST_SOURCES})

    target_link_libraries(
        ${_expected_name}
        PRIVATE
        lumex::expected
    )

    lumex_test_use_gtest(${_expected_name} CXX_STANDARD ${_expected_std})
    if(_expected_suffix)
        lumex_gtest_discover_tests(${_expected_name} TEST_SUFFIX "${_expected_suffix}")
    else()
        lumex_gtest_discover_tests(${_expected_name})
    endif()
endforeach()
