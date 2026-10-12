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
# STANDARDS narrows the module's row to 11 17 20 for this directory. Nothing in
# its tests or in the sources they test differs at C++14 from the next lower
# standard (the text of the directory's own files after preprocessing is the
# same, no __cplusplus threshold or LUMEX_HAS_* feature of those standards is
# used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(StringText
    MODULE string
    LINK lumex::string lumex::string_view
    SOURCES LumexTextConstraintsTestHelpers.hpp ../LumexStringTestFixtures.hpp
    STANDARDS 11 17 20
)
