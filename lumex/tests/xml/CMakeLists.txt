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

# Same sources, pinned to C++20. LumexXml is a C++11 library, but consumers
# such as PeakExpertWeb compile its headers (and, when embedded, its .cpp
# files) at C++20. A block-scope LUMEX_CONST_NUM (static inline) in
# XmlUtils.hpp compiled at C++11 and broke every C++17+ build; this suite
# keeps the C++20 path compiling. TEST_SUFFIX keeps CTest names distinct.
set(XML_CXX20_TEST_NAME LumexXmlCxx20Tests)

add_executable(
    ${XML_CXX20_TEST_NAME}
    LumexXml.tests.cpp
)

target_link_libraries(
    ${XML_CXX20_TEST_NAME}
    PRIVATE
    lumex::xml
)

lumex_test_use_gtest(${XML_CXX20_TEST_NAME} CXX_STANDARD 20)
gtest_discover_tests(${XML_CXX20_TEST_NAME} TEST_SUFFIX ".cxx20")

target_link_options(${XML_CXX20_TEST_NAME} PRIVATE -Wl,--no-as-needed)
