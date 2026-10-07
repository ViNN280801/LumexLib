# lumex/tests/core/unicode/CMakeLists.txt

# The tests follow the source tree (lumex/core/unicode):
#   utf/      the counters, writers and decoders of utf/LumexUtf.hpp
#   convert/  to_utf8 and to_wide of convert/LumexUnicodeConvert.hpp
#   this one  LumexUnicodeGlobalNames.cxx11.tests.cpp, the test of the module
#             umbrella, and LumexUnicodeTestSupport.hpp (the reference
#             encoders, the code point generator and the recording traits),
#             which both directories use
# The module has no code that depends on a later standard, so every directory
# has one suite, at C++11 (lumex/tests/LumexTestStandards.cmake):
# LumexUnicodeCxx11Tests, LumexUnicodeUtfCxx11Tests and
# LumexUnicodeConvertCxx11Tests. CTest names carry the prefix of the
# directory ("unicode.", "unicode.utf.", "unicode.convert.").
# LumexUnicodeGlobalNames.cxx11.tests.cpp declares global names spelled like
# the module's and uses them unqualified: every suite stops building if a
# header of the module puts a name into the global namespace.
# The XML module's own encoding tests (the byte order mark, UTF-16, UTF-32 and
# Latin-1 through XmlDocument) are in lumex/tests/xml.
lumex_add_standard_suites(Unicode
    MODULE unicode
    LINK lumex::unicode
    SOURCES LumexUnicodeTestSupport.hpp
)

add_subdirectory(convert)
add_subdirectory(utf)
