# lumex/tests/core/unicode/utf/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexUnicodeUtfCxx11Tests  LumexUtf.cxx11.tests.cpp: the counters, writers
#                              and decoders for UTF-8, UTF-16, UTF-32, Latin-1
#                              and wchar_t, with the invalid-input rules the
#                              header documents. The expected bytes come from
#                              the reference encoders of
#                              ../LumexUnicodeTestSupport.hpp. The UTF-8
#                              decoder is also compared with a reference
#                              written from Table 3-7 of the Unicode standard
#                              and with LumexUtf8OracleData.hpp, the corpus
#                              that CPython's bytes.decode ('utf-8',
#                              'replace') decoded (generated once; the
#                              expected code points are what it gives with
#                              the U+FFFD that it writes for each ill-formed
#                              subsequence left out, a U+FFFD encoded in the
#                              input is kept).
lumex_add_standard_suites(UnicodeUtf
    MODULE unicode
    LINK lumex::unicode
    SOURCES ../LumexUnicodeTestSupport.hpp LumexUtf8OracleData.hpp
)
