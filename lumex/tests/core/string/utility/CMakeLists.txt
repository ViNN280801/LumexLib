# lumex/tests/core/string/utility/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexStringUtilityCxx11Tests  *.cxx11.tests.cpp: format::stringify
#   LumexStringUtilityCxx14Tests  the C++11 files at C++14
#   LumexStringUtilityCxx17Tests  the C++11 files at C++17
#   LumexStringUtilityCxx20Tests  + *.cxx20.tests.cpp: stringify_v2 and the
#                                 stream concepts
# LumexStringTestFixtures.hpp of the module's directory holds the fixture and
# the types the files of several standards share.
lumex_add_standard_suites(StringUtility
    MODULE string
    LINK lumex::string lumex::string_view
    SOURCES ../LumexStringTestFixtures.hpp
)
