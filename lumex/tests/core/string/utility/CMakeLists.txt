# lumex/tests/core/string/utility/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexStringUtilityCxx11Tests  *.cxx11.tests.cpp: format::stringify and
#                                 stringify_v2
#   LumexStringUtilityCxx14Tests  the C++11 files at C++14
#   LumexStringUtilityCxx17Tests  the C++11 files at C++17
#   LumexStringUtilityCxx20Tests  + *.cxx20.tests.cpp: the stream concepts and
#                                 the tie of the stringify_v2 constraint to
#                                 the concept AllStringifiable
# LumexStringTestFixtures.hpp of the module's directory holds the fixture and
# the types the files of several standards share.
lumex_add_standard_suites(StringUtility
    MODULE string
    LINK lumex::string lumex::string_view
    SOURCES ../LumexStringTestFixtures.hpp
)
