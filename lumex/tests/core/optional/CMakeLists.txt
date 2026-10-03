# lumex/tests/core/optional/CMakeLists.txt

# optional has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexOptionalCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(Optional
    MODULE optional
    LINK
        lumex::optional
)
