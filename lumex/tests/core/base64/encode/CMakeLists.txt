# lumex/tests/core/base64/encode/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexBase64EncodeCxx11Tests  LumexBase64Encoder.cxx11.tests.cpp: the
#                                pointer and size core and the std::vector
#                                overload; LumexBase64EncoderSpan.cxx11.tests.cpp:
#                                the span overload;
#                                LumexBase64EncoderStringView.cxx11.tests.cpp:
#                                the string overload (string_type_t:
#                                lumex_string_view below C++17, std::string_view
#                                from C++17)
#   LumexBase64EncodeCxx17Tests  + LumexBase64Encoder.cxx17.tests.cpp:
#                                std::string_view arguments
#   LumexBase64EncodeCxx20Tests  + LumexBase64Encoder.cxx20.tests.cpp: std::span
# Every suite links the same exported pointer and size functions, whatever
# standard the library itself was built with.
lumex_add_standard_suites(Base64Encode
    MODULE base64
    LINK lumex::base64
    SOURCES ../LumexBase64TestFixtures.hpp
)
