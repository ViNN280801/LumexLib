# lumex/tests/core/base64/validate/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexBase64ValidateCxx11Tests  LumexBase64Validator.cxx11.tests.cpp: the
#                                  pointer and size core and std::string
#                                  arguments;
#                                  LumexBase64ValidatorStringView.cxx11.tests.cpp:
#                                  the string overload (string_type_t:
#                                  lumex_string_view in every standard)
#   LumexBase64ValidateCxx17Tests  + LumexBase64Validator.cxx17.tests.cpp:
#                                  std::string_view arguments
#   LumexBase64ValidateCxx20Tests  the same files at C++20
lumex_add_standard_suites(Base64Validate
    MODULE base64
    LINK lumex::base64
    SOURCES ../LumexBase64TestFixtures.hpp
)
