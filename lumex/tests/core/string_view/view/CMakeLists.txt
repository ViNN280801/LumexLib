# lumex/tests/core/string_view/view/CMakeLists.txt

# string_view has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexStringViewViewCxx11Tests builds every *.cxx11.tests.cpp of this
# directory.
lumex_add_standard_suites(StringViewView
    MODULE string_view
    LINK
        lumex::string_view
)
