# lumex/tests/core/base64/decode/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexBase64DecodeCxx11Tests  LumexBase64Decoder.cxx11.tests.cpp: the
#                                pointer and size core, the std::string and
#                                std::vector overloads
#   LumexBase64DecodeCxx17Tests  + LumexBase64Decoder.cxx17.tests.cpp:
#                                std::string_view
#   LumexBase64DecodeCxx20Tests  the same files at C++20
lumex_add_standard_suites(Base64Decode
    MODULE base64
    LINK lumex::base64
    SOURCES ../LumexBase64TestFixtures.hpp
)
