# lumex/tests/core/string_view/view/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexStringViewViewCxx11Tests  every *.cxx11.tests.cpp of this directory
#                                  (LumexStringViewParameter: the view as the
#                                  by-value text parameter of a function)
#   LumexStringViewViewCxx17Tests  the same files at C++17 and
#                                  LumexStringViewStd.cxx17.tests.cpp: the
#                                  conversions to and from std::string_view
#                                  and std::wstring_view, the comparisons with
#                                  them and the overload sets next to them
#   LumexStringViewViewCxx20Tests  the same files at C++20
lumex_add_standard_suites(StringViewView
    MODULE string_view
    LINK
        lumex::string_view
)
