# lumex/tests/core/string_view/view/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexStringViewViewCxx11Tests  every *.cxx11.tests.cpp of this directory
#                                  (the class, the searches against
#                                  std::basic_string, the order, the hash, the
#                                  stream inserter, constexpr under the rules
#                                  of C++11, the view as the by-value text
#                                  parameter of a function)
#   LumexStringViewViewCxx14Tests  the same files at C++14 and
#                                  LumexStringViewConstexpr.cxx14.tests.cpp:
#                                  the modifiers in constant expressions
#   LumexStringViewViewCxx17Tests  the same files at C++17 and the .cxx17
#                                  files: the conversions to and from
#                                  std::string_view and std::wstring_view, the
#                                  comparisons with them, the differential
#                                  tests against them, constexpr reverse
#                                  iterators
#   LumexStringViewViewCxx20Tests  the same files at C++20 and the .cxx20
#                                  files: operator<=>, the ranges concepts and
#                                  the iterator-pair constructor
lumex_add_standard_suites(StringViewView
    MODULE string_view
    LINK
        lumex::string_view
    SOURCES LumexStringViewTestSupport.hpp
)
