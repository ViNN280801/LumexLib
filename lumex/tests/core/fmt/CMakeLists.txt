# lumex/tests/core/fmt/CMakeLists.txt

# LumexFormat (lumex/core/fmt): the same sources at every standard
# with its own code path. C++11 is the floor (printf / strtod float
# fallback, run-time format string checks); C++17 switches floats to
# std::to_chars; C++20 adds the consteval format string check and the
# std::format cross-check (LumexFormatStdCompare.tests.cpp).
set(LUMEX_FORMAT_TEST_SOURCES
    LumexFormatApi.tests.cpp
    LumexFormatChrono.tests.cpp
    LumexFormatDifferential.tests.cpp
    LumexFormatEdgeCases.tests.cpp
    LumexFormatFloat.tests.cpp
    LumexFormatMinMaxMacros.tests.cpp
    LumexFormatParse.tests.cpp
    LumexFormatPerf.tests.cpp
    LumexFormatRanges.tests.cpp
    LumexFormatSpecs.tests.cpp
    LumexFormatStdCompare.tests.cpp
    LumexFormatTestHelpers.hpp
    LumexFormatTypes.tests.cpp
)

foreach(_format_suite IN ITEMS
        "LumexFormatTests;11;"
        "LumexFormatCxx17Tests;17;.cxx17"
        "LumexFormatCxx20Tests;20;.cxx20")
    list(GET _format_suite 0 _format_name)
    list(GET _format_suite 1 _format_std)
    list(GET _format_suite 2 _format_suffix)

    add_executable(${_format_name} ${LUMEX_FORMAT_TEST_SOURCES})

    target_link_libraries(
        ${_format_name}
        PRIVATE
        lumex::fmt
        lumex::string_view
        lumex::reflection
    )

    lumex_test_use_gtest(${_format_name} CXX_STANDARD ${_format_std})
    if(_format_suffix)
        lumex_gtest_discover_tests(${_format_name} TEST_SUFFIX "${_format_suffix}")
    else()
        lumex_gtest_discover_tests(${_format_name})
    endif()
endforeach()
