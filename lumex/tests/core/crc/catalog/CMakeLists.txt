# lumex/tests/core/crc/catalog/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexCrcCatalogCxx11Tests  LumexCrcCatalog.cxx11.tests.cpp,
#                              LumexCrcCatalogSpan.cxx11.tests.cpp (the span
#                              overloads) and
#                              LumexCrcCatalogStringView.cxx11.tests.cpp (the
#                              string overloads: lumex_string_view below
#                              C++17, std::string_view from C++17; and
#                              append_crc_least_significant_byte_first): the
#                              library is compiled as C++11 and every suite
#                              links it
#   LumexCrcCatalogCxx14Tests  the same files at C++14
#   LumexCrcCatalogCxx17Tests  + LumexCrcCatalog.cxx17.tests.cpp:
#                              std::string_view arguments of the catalogue
#   LumexCrcCatalogCxx20Tests  + LumexCrcCatalogStdSpan.cxx20.tests.cpp: the
#                              std::span overloads
lumex_add_standard_suites(CrcCatalog
    MODULE crc
    LINK lumex::crc
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
