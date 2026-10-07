# lumex/tests/xml/xpath/query/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexXmlXpathQueryCxx11Tests  LumexXmlXPathNumbers.cxx11.tests.cpp: the
#                                 exact double comparisons of the XPath number
#                                 rules (the library writes them through one
#                                 helper, core/math exactly_equal, so the
#                                 compiler's -Wfloat-equal stays quiet);
#                                 LumexXmlXPathRelational.cxx11.tests.cpp:
#                                 the operators <, <=, > and >= on numbers,
#                                 strings, booleans and node-sets;
#                                 LumexXmlXPathNaN.cxx11.tests.cpp: the places
#                                 that produce a NaN
#   LumexXmlXpathQueryCxx17Tests  the same files at C++17
#   LumexXmlXpathQueryCxx20Tests  the same files at C++20
# LumexXmlTestFixtures.hpp of the module's directory holds the fixture.
lumex_add_standard_suites(XmlXpathQuery
    MODULE xml
    LINK
        lumex::xml
    SOURCES ../../LumexXmlTestFixtures.hpp
    TARGETS_VAR _xml_xpath_query_suites
)

foreach(_suite IN LISTS _xml_xpath_query_suites)
    target_link_options(${_suite} PRIVATE -Wl,--no-as-needed)
endforeach()
