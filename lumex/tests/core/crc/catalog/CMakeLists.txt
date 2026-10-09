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
# STANDARDS narrows the module's row to 11 17 20 for this directory. Nothing in
# its tests or in the sources they test differs at C++14 from the next lower
# standard (the text of the directory's own files after preprocessing is the
# same, no __cplusplus threshold or LUMEX_HAS_* feature of those standards is
# used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(CrcCatalog
    MODULE crc
    LINK lumex::crc
    DISCOVER_ARGS DISCOVERY_TIMEOUT 60
    STANDARDS 11 17 20
)
