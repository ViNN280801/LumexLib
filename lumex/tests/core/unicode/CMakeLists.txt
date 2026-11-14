# lumex/tests/core/unicode/CMakeLists.txt

# The tests follow the source tree (lumex/core/unicode):
#   utf/      the counters, writers and decoders of utf/LumexUtf.hpp
#   convert/  to_utf8 and to_wide of convert/LumexUnicodeConvert.hpp
#   this one  LumexUnicodeGlobalNames.cxx11.tests.cpp, the test of the module
#             umbrella, and LumexUnicodeTestSupport.hpp (the reference
#             encoders, the code point generator and the recording traits),
#             which both directories use
# The module is C++11 except for the char8_t overloads of the UTF-8 validation
# (utf/LumexUtf8Validate.hpp): the utf directory is its own test module
# (unicode.utf, C++11 and C++20, and the no_simd variant, see utf/CMakeLists.txt),
# this directory and convert/ are C++11 (lumex/tests/LumexTestStandards.cmake):
# LumexUnicodeCxx11Tests, LumexUnicodeConvertCxx11Tests. CTest names carry the
# prefix of the directory ("unicode.", "unicode.utf.", "unicode.convert.") and
# the standard as the last part.
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
