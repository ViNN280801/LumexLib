# lumex/tests/core/environment/CMakeLists.txt

# environment has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexEnvironmentCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(Environment
    MODULE environment
    LINK
        lumex::environment
)
