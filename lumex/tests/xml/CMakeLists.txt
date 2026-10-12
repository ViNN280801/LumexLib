# lumex/tests/xml/CMakeLists.txt

# The tests follow the source tree (lumex/xml): this directory keeps the tests
# that exercise the document, node, attribute and writer classes together
# through the umbrella, and xpath/query holds the XPath number rules. One suite
# per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexXmlCxx11Tests  LumexXml.cxx11.tests.cpp,
#                       LumexXmlGlobalNames.cxx11.tests.cpp and
#                       LumexXmlEncodings.cxx11.tests.cpp (UTF-16, UTF-32 and
#                       Latin-1 through load_buffer, load and save: the byte
#                       order mark, the detection of the encoding, output
#                       longer than the serializer's buffer) and
#                       LumexXmlRanges.cxx11.tests.cpp (XmlNode::children and
#                       attributes return an iterator_range) and
#                       LumexXmlParserOptions.cxx11.tests.cpp (the parse
#                       options that select a string conversion of the parser)
#                       and LumexXmlStringView.cxx11.tests.cpp (the
#                       string_view_t overloads: lumex_string_view in
#                       every standard, so std::string and sized-view
#                       arguments work in every suite)
#   LumexXmlCxx17Tests  + LumexXml.cxx17.tests.cpp: std::string_view
#                       arguments
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

add_subdirectory(xpath)
