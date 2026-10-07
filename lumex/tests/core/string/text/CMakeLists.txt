# lumex/tests/core/string/text/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexStringTextCxx11Tests  *.cxx11.tests.cpp: text::join / quote* /
#                              to_case_insensitive and the SFINAE constraints
#                              of join / quote
#   LumexStringTextCxx14Tests  the C++11 files at C++14
#   LumexStringTextCxx17Tests  + LumexJoin.cxx17.tests.cpp: a std::string_view
#                              separator below C++20
#   LumexStringTextCxx20Tests  + *.cxx20.tests.cpp: the std::ranges join /
#                              quote and their requires-clauses
# LumexTextConstraintsTestHelpers.hpp holds what the constraint tests of
# several standards share.
lumex_add_standard_suites(StringText
    MODULE string
    LINK lumex::string lumex::string_view
    SOURCES LumexTextConstraintsTestHelpers.hpp ../LumexStringTestFixtures.hpp
)
