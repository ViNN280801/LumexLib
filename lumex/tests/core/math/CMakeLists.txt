# lumex/tests/core/math/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexMathCxx11Tests  LumexMath.cxx11.tests.cpp: LumexMath works from C++11
#   LumexMathCxx17Tests  the same file at C++17
#   LumexMathCxx20Tests  + LumexMath.cxx20.tests.cpp: std::views (filter views
#                        that are not const-iterable, sentinel ends)
lumex_add_standard_suites(Math
    MODULE math
    LINK
        lumex::math
)
