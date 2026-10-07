# lumex/tests/core/unicode/utf/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexUnicodeUtfCxx11Tests  LumexUtf.cxx11.tests.cpp: the counters, writers
#                              and decoders for UTF-8, UTF-16, UTF-32, Latin-1
#                              and wchar_t, with the invalid-input rules the
#                              header documents. The expected bytes come from
#                              the reference encoders of
#                              ../LumexUnicodeTestSupport.hpp.
lumex_add_standard_suites(UnicodeUtf
    MODULE unicode
    LINK lumex::unicode
    SOURCES ../LumexUnicodeTestSupport.hpp
)
