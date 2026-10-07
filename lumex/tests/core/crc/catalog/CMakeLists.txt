# lumex/tests/core/crc/catalog/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexCrcCatalogCxx14Tests  LumexCrcCatalog.cxx14.tests.cpp: the catalogue
#                              checks use std::make_index_sequence
#   LumexCrcCatalogCxx17Tests  + LumexCrcCatalog.cxx17.tests.cpp: the
#                              std::string_view overloads of the catalogue
#   LumexCrcCatalogCxx20Tests  the same files at C++20
lumex_add_standard_suites(CrcCatalog
    MODULE crc
    LINK lumex::crc
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
)
