# lumex/tests/xml/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexXmlCxx11Tests  LumexXml.cxx11.tests.cpp,
#                       LumexXmlGlobalNames.cxx11.tests.cpp and
#                       LumexXmlXPathNumbers.cxx11.tests.cpp (the exact double
#                       comparisons of the number rules)
#   LumexXmlCxx17Tests  + LumexXml.cxx17.tests.cpp: the std::string_view
#                       overloads
#   LumexXmlCxx20Tests  the same files at C++20. LumexXml is a C++11 library,
#                       but consumers compile its headers (and, when
#                       embedded, its .cpp files) at C++20: a block-scope
#                       LUMEX_CONST_NUM (static inline) in XmlUtils.hpp once
#                       compiled at C++11 and broke every C++17+ build.
# LumexXmlTestFixtures.hpp holds the fixture and helpers the files of every
# standard share.
#
# LumexXmlGlobalNames.cxx11.tests.cpp includes the XML and base64 umbrellas
# and declares global names that a file-scope using-directive in their
# headers would make ambiguous. It uses only the declarations of base64 (no
# call), so the suites link lumex::xml alone and build without the base64
# module.
lumex_add_standard_suites(Xml
    MODULE xml
    LINK
        lumex::xml
    SOURCES LumexXmlTestFixtures.hpp
    TARGETS_VAR _xml_suites
)

foreach(_suite IN LISTS _xml_suites)
    target_link_options(${_suite} PRIVATE -Wl,--no-as-needed)
endforeach()
