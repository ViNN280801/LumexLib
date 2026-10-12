# lumex/tests/core/base64/decode/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexBase64DecodeCxx11Tests  LumexBase64Decoder.cxx11.tests.cpp: the
#                                pointer and size core and the string
#                                overloads taking std::string arguments;
#                                LumexBase64DecoderStringView.cxx11.tests.cpp:
#                                the string overloads (string_type_t:
#                                lumex_string_view in every standard)
#   LumexBase64DecodeCxx17Tests  + LumexBase64Decoder.cxx17.tests.cpp:
#                                std::string_view arguments
#   LumexBase64DecodeCxx20Tests  the same files at C++20
lumex_add_standard_suites(Base64Decode
    MODULE base64
    LINK lumex::base64
    SOURCES ../LumexBase64TestFixtures.hpp
)
