# lumex/tests/core/unicode/convert/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexUnicodeConvertCxx11Tests  LumexUnicodeConvert.cxx11.tests.cpp:
#                                  to_utf8 and to_wide, on both widths of
#                                  wchar_t
lumex_add_standard_suites(UnicodeConvert
    MODULE unicode
    LINK lumex::unicode
    SOURCES ../LumexUnicodeTestSupport.hpp
)
