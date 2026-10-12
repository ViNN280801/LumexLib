# lumex/tests/core/math/ops/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
#   LumexMathOpsCxx11Tests  LumexMath.cxx11.tests.cpp: LumexMath works from
#                           C++11, and LumexMathExactlyEqual.cxx11.tests.cpp:
#                           the exact floating-point comparison
#   LumexMathOpsCxx17Tests  the same files at C++17
#   LumexMathOpsCxx20Tests  + LumexMath.cxx20.tests.cpp: std::views (filter
#                           views that are not const-iterable, sentinel ends)
lumex_add_standard_suites(MathOps
    MODULE math
    LINK
        lumex::math
)
