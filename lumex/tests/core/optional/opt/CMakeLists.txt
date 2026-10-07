# lumex/tests/core/optional/opt/CMakeLists.txt

# optional has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexOptionalOptCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(OptionalOpt
    MODULE optional
    LINK
        lumex::optional
)
