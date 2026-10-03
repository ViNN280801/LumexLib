# lumex/tests/core/temporary/CMakeLists.txt

# temporary has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexTemporaryCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(Temporary
    MODULE temporary
    LINK
        lumex::temporary
)
