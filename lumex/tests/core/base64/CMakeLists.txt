# lumex/tests/core/base64/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexBase64Cxx11Tests  LumexBase64.cxx11.tests.cpp: the pointer and size
#                          core, the std::string and std::vector overloads
#   LumexBase64Cxx17Tests  + LumexBase64.cxx17.tests.cpp: std::string_view
#   LumexBase64Cxx20Tests  + LumexBase64.cxx20.tests.cpp: std::span
# Every suite links the same exported pointer and size functions, whatever
# standard the library itself was built with. LumexBase64TestFixtures.hpp
# holds the fixtures the files of every standard share.
lumex_add_standard_suites(Base64
    MODULE base64
    LINK lumex::base64
    SOURCES LumexBase64TestFixtures.hpp
)
