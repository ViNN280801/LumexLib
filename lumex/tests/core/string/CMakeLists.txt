# lumex/tests/core/string/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexStringifyCxx11Tests  *.cxx11.tests.cpp: format::stringify,
#                             text::join / quote* / to_case_insensitive and
#                             the SFINAE constraints of join / quote
#   LumexStringifyCxx14Tests  the C++11 files at C++14
#   LumexStringifyCxx17Tests  + LumexJoin.cxx17.tests.cpp: a std::string_view
#                             separator below C++20
#   LumexStringifyCxx20Tests  + *.cxx20.tests.cpp: stringify_v2, the stream
#                             concepts, the std::ranges join / quote and
#                             their requires-clauses
# LumexStringTestFixtures.hpp and LumexTextConstraintsTestHelpers.hpp hold
# what the files of several standards share.
lumex_add_standard_suites(Stringify
    MODULE string
    LINK lumex::string lumex::string_view
    SOURCES LumexStringTestFixtures.hpp LumexTextConstraintsTestHelpers.hpp
)
