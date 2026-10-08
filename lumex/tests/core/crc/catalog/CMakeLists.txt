# lumex/tests/core/crc/catalog/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexCrcCatalogCxx11Tests  LumexCrcCatalog.cxx11.tests.cpp and
#                              LumexCrcCatalogSpan.cxx11.tests.cpp: the library
#                              is compiled as C++11 and every suite links it
#   LumexCrcCatalogCxx14Tests  the same files at C++14
#   LumexCrcCatalogCxx17Tests  + LumexCrcCatalog.cxx17.tests.cpp: the
#                              std::string_view overloads of the catalogue
#   LumexCrcCatalogCxx20Tests  + LumexCrcCatalogStdSpan.cxx20.tests.cpp: the
#                              std::span overloads
lumex_add_standard_suites(CrcCatalog
    MODULE crc
    LINK lumex::crc
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
