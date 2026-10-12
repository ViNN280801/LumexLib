# lumex/tests/core/temporary/tmp/CMakeLists.txt

# temporary has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexTemporaryTmpCxx11Tests builds every *.cxx11.tests.cpp of this
# directory.
lumex_add_standard_suites(TemporaryTmp
    MODULE temporary
    LINK
        lumex::temporary
)
